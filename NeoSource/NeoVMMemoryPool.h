#pragma once

#include <chrono>
#include <cstdlib>
#include <deque>
#include <new>
#include <type_traits>
#include <vector>

namespace NeoScript
{

typedef std::chrono::steady_clock NeoPoolClock;

// 아래 체인 조작들은 페이지가 "가득 참 / 완전히 빔" 경계를 넘을 때만 도는 희귀 경로다
// (512 노드 페이지면 512번에 한 번). 인라인시키면 Receive/Confer 본체만 커져서 진짜
// 핫패스의 I-cache 를 축낸다 — 호출로 두는 편이 빠르다.
#if defined(_MSC_VER)
#define NEOPOOL_COLD __declspec(noinline)
#else
#define NEOPOOL_COLD __attribute__((noinline))
#endif

// #pragma pack(1) 을 쓰지 않는다.
// 팩하면 STNode 의 payload 가 u32 dwpFlag 뒤 4바이트 경계에 놓여, 8바이트 정렬이 필요한
// 타입(포인터/std::string 을 가진 것들)이 미정렬 주소에 생긴다 — UB 이고 ARM 에서는 폴트,
// x64 에서도 컴파일러가 정렬 가정 명령(movaps 등)을 쓰면 깨진다.
// 노드당 몇 바이트 늘지만 정렬을 지키는 쪽이 맞다.

// ---------------------------------------------------------------------------
// 페이지 기반 오브젝트 풀 (공통 설명)
//
// 노드를 페이지 단위로 잡고, 반납된 노드는 free 리스트로 재사용한다.
// free 리스트를 풀 전체에 하나만 두면 "이 페이지의 유휴 노드" 를 골라낼 수 없어
// 페이지를 영원히 못 돌려준다. 그래서 free 리스트를 페이지마다 둔다.
//
//  - 각 페이지: 자기 free 리스트 + usedCount
//  - avail 체인 : 유휴 노드가 남은 페이지들. 양방향이다 — 회수할 때 O(1) 로 빠지려고.
//  - empty 체인 : usedCount==0 인 페이지들을 "비워진 순서" 로 이은 FIFO. 양방향.
//  - 노드 헤더의 dwpFlag 에 소속 페이지 슬롯 번호를 넣어 Confer 에서 O(1) 로 찾는다
//
// [증분 회수 — 매 프레임 조금씩]
// Collect() 는 페이지 전체를 훑지 않는다. empty 체인의 head 가 항상 "가장 오래 비어
// 있는 페이지" 이므로, head 가 아직 만료 전이면 거기서 멈춘다(O(1)). 해제도 호출자가
// 준 페이지 예산만큼만 하고 나머지는 다음 호출로 넘긴다. 페이지 하나를 돌려주는 비용도
// O(1) 이다 — 체인에서 언링크만 하고, 예전처럼 avail 체인을 O(P) 로 다시 만들지 않는다.
// 결과적으로 회수 시점에 몰리는 스파이크가 없고 프레임당 비용에 상한이 생긴다.
//
// [빈 시각을 언제 찍나]
// Confer 는 시계를 읽지 않는다(반납 핫패스에서 QueryPerformanceCounter 를 부르게 된다).
// 대신 empty 체인 꼬리쪽의 "아직 안 찍은 연속 구간" 을 Collect 가 한 번에 찍는다.
// 체인이 비워진 순서라 찍힌 시각도 head→tail 로 단조 증가하고, 그래서 head 만 봐도 된다.
// 보유 시간은 "Collect 가 빈 걸 처음 본 시점" 부터 흐른다 — 회수를 늦추는 방향이라 안전.
//
// [페이지 헤더]
// 헤더(STPool)는 deque 에 모아 두고 슬롯 번호로 재사용한다. 페이지 데이터를 해제해도
// 헤더 자체는 남는다(페이지당 수십 바이트). 주소가 고정이라 체인 포인터와 슬롯 표를
// 재구성할 필요가 없고, 그래서 회수가 O(1) 이 된다.
// ---------------------------------------------------------------------------

// 성능 최적화를 위해서 생성자/소멸자 지원하지 않음.
// 생성자 소멸자 처리가 필요한 풀은 CNVMInstPool 을 사용하면 됨.
template <typename T, int iBlkSize = 128>
class CNVMAllocPool
{
	struct STNode
	{
		u32 dwpFlag;            // 사용 중일 때 = 소속 페이지 슬롯 번호
		// 바이트 배열이라 컴파일러가 T 의 정렬을 알 수 없다. pack 을 빼는 것만으로는
		// dwpFlag 뒤 4바이트 경계에 그대로 놓이므로 alignas 로 명시한다.
		alignas(T) u8 data[sizeof(T)];
	};

	// 유휴일 때만 m_pNext 를 쓴다. dwpFlag 와 data 앞부분에 겹치지만,
	// 유휴 노드의 payload 는 죽은 값이라 상관없다(원래 구조와 동일).
	// dwpFlag 는 Receive 에서 쓰고 Confer 에서 읽으므로 필요한 순간엔 항상 유효하다.
	union SNodePool
	{
		SNodePool*	m_pNext;
		STNode		m_sObj;
	};

	struct STPool
	{
		SNodePool*	pData;          // NULL = 데이터가 해제된 헤더(슬롯 재사용 대기)
		SNodePool*	pFreeHead;      // 이 페이지의 유휴 노드
		STPool*		pPrevAvail;     // avail 체인(양방향)
		STPool*		pNextAvail;
		STPool*		pPrevEmpty;     // empty 체인(양방향, 비워진 순서)
		STPool*		pNextEmpty;
		int			usedCount;
		u32			slot;           // m_sPageSlots / m_sPages 에서의 위치
		bool		inAvail;
		bool		emptyStamped;   // 빈 시각을 찍었는가(Confer 는 안 찍는다)
		NeoPoolClock::time_point emptySince;
	};

	std::deque<STPool>		m_sPages;        // 페이지 헤더 저장소(주소 고정, 줄지 않음)
	std::vector<STPool*>	m_sPageSlots;    // slot -> 헤더. 핫패스용 1단 역참조.
	std::vector<u32>		m_sFreeSlots;    // 재사용 가능한 슬롯 번호
	STPool*					m_pAvailHead = NULL;
	STPool*					m_pEmptyHead = NULL;   // 가장 먼저 빈 페이지 = 가장 먼저 만료
	STPool*					m_pEmptyTail = NULL;
	STPool*					m_pUnstamped = NULL;   // 빈 시각을 아직 안 찍은 첫 페이지

	size_t m_nReservedBytes = 0;

	static void ReleasePageData(STPool& page) { free(page.pData); }

	// --- 체인 조작 (양쪽 풀 클래스에 같은 형태로 들어간다) ---
	NEOPOOL_COLD void PushEmpty(STPool* page)
	{
		page->pPrevEmpty = m_pEmptyTail;
		page->pNextEmpty = NULL;
		if (m_pEmptyTail != NULL)
			m_pEmptyTail->pNextEmpty = page;
		else
			m_pEmptyHead = page;
		m_pEmptyTail = page;

		page->emptyStamped = false;
		if (m_pUnstamped == NULL)
			m_pUnstamped = page;   // 여기부터 꼬리까지가 아직 안 찍은 구간
	}

	NEOPOOL_COLD void UnlinkEmpty(STPool* page)
	{
		if (m_pUnstamped == page)
			m_pUnstamped = page->pNextEmpty;   // 뒤쪽도 전부 안 찍힌 것들이다
		if (page->pPrevEmpty != NULL)
			page->pPrevEmpty->pNextEmpty = page->pNextEmpty;
		else
			m_pEmptyHead = page->pNextEmpty;
		if (page->pNextEmpty != NULL)
			page->pNextEmpty->pPrevEmpty = page->pPrevEmpty;
		else
			m_pEmptyTail = page->pPrevEmpty;
	}

	NEOPOOL_COLD void PushAvail(STPool* page)
	{
		page->pPrevAvail = NULL;
		page->pNextAvail = m_pAvailHead;
		if (m_pAvailHead != NULL)
			m_pAvailHead->pPrevAvail = page;
		m_pAvailHead = page;
		page->inAvail = true;
	}

	NEOPOOL_COLD void UnlinkAvail(STPool* page)
	{
		if (page->inAvail == false)
			return;
		if (page->pPrevAvail != NULL)
			page->pPrevAvail->pNextAvail = page->pNextAvail;
		else
			m_pAvailHead = page->pNextAvail;
		if (page->pNextAvail != NULL)
			page->pNextAvail->pPrevAvail = page->pPrevAvail;
		page->inAvail = false;
	}

	void clear()
	{
		for (auto it = m_sPages.begin(); it != m_sPages.end(); ++it)
			if ((*it).pData != NULL)
				free((*it).pData);
		m_sPages.clear();
		m_sPageSlots.clear();
		m_sFreeSlots.clear();
		m_pAvailHead = NULL;
		m_pEmptyHead = NULL;
		m_pEmptyTail = NULL;
		m_pUnstamped = NULL;
		m_nReservedBytes = 0;
	}

	// alloc()이 장부 확장 중 예외를 전파해도 malloc 블록을 자동으로 놓는다.
	// unique_ptr의 배열/커스텀 deleter 문법을 피하고 풀에 필요한 동작만 둔다.
	struct AllocDataGuard
	{
		SNodePool* pData;

		explicit AllocDataGuard(int count)
			: pData((SNodePool*)malloc(sizeof(SNodePool) * count))
		{
			if (pData == NULL)
				throw std::bad_alloc();
		}
		~AllocDataGuard() { free(pData); }
		AllocDataGuard(const AllocDataGuard&) = delete;
		AllocDataGuard& operator=(const AllocDataGuard&) = delete;

		SNodePool* release()
		{
			SNodePool* result = pData;
			pData = NULL;
			return result;
		}
	};

	NEOPOOL_COLD void alloc()
	{
		// [순서가 예외 안전을 만든다] 페이지 데이터를 **먼저** 잡고, 장부
		// (m_sPages / m_sPageSlots / m_sFreeSlots)는 **나중에** 건드린다.
		// 그러면 어느 쪽이 실패해도 되돌릴 것이 없어 try/catch 가 필요 없다.
		//   - 데이터 확보 실패 → 장부를 아직 안 만졌으므로 그냥 던진다.
		//   - 장부 확장 실패(deque/vector 증설) → 지역 소유 가드가 데이터를 해제한다.
		// malloc 은 던지지 않고 null 을 주므로, 검사해서 new[] 쪽(CNVMInstPool)과
		// 같은 계약(std::bad_alloc)으로 맞춘다. 검사하지 않으면 아래 free-list
		// 초기화가 널을 역참조해 프로세스가 죽는다.
		AllocDataGuard data(m_iBlkSize);

		STPool* page;
		if (m_sFreeSlots.empty())
		{
			// m_sPageSlots가 먼저 용량을 확보하게 한다. 이 단계가 던지면 아직
			// deque 헤더도 늘지 않았고 data가 자동 해제된다.
			m_sPageSlots.reserve(m_sPageSlots.size() + 1);
			// deque 는 push_back 으로 기존 원소의 주소를 무효화하지 않는다.
			m_sPages.push_back(STPool());
			page = &m_sPages.back();
			page->slot = (u32)m_sPageSlots.size();
			m_sPageSlots.push_back(page);
		}
		else
		{
			const u32 slot = m_sFreeSlots.back();
			page = m_sPageSlots[slot];   // 데이터만 해제됐던 헤더를 그대로 다시 쓴다
			page->slot = slot;
			m_sFreeSlots.pop_back();     // 던질 일이 끝난 뒤에 슬롯을 소비한다
		}

		// 여기서부터는 던지는 연산이 없다 — 소유권을 페이지로 넘긴다.
		page->pData = data.release();
		page->usedCount = 0;

		SNodePool* pData = page->pData;
		for (int i = m_iBlkSize - 2; i >= 0; i--)
			pData[i].m_pNext = &pData[i + 1];
		pData[m_iBlkSize - 1].m_pNext = NULL;
		page->pFreeHead = &pData[0];

		page->inAvail = false;
		PushAvail(page);
		// usedCount==0 이면 반드시 empty 체인에 있다는 불변식을 지킨다.
		// (바로 뒤 Receive 가 노드를 하나 가져가면서 다시 빠진다)
		PushEmpty(page);

		m_nReservedBytes += sizeof(SNodePool) * (size_t)m_iBlkSize;
	}

public:
	int m_iBlkSize = 1;
	u32 _dwLastID = 0;

	CNVMAllocPool() { m_iBlkSize = iBlkSize; }
	virtual ~CNVMAllocPool() { clear(); }

	// payload 의 "오프셋" 이 T 의 정렬을 만족해야 한다(구조체 전체 정렬만으로는 부족).
	static const size_t kDataOffset = offsetof(SNodePool, m_sObj) + offsetof(STNode, data);
	static_assert(kDataOffset % alignof(T) == 0,
		"pool node payload offset must satisfy alignof(T)");

	inline T* Receive()
	{
		if (m_pAvailHead == NULL)
			alloc();

		STPool* page = m_pAvailHead;
		SNodePool* __p = page->pFreeHead;
		page->pFreeHead = __p->m_pNext;
		if (page->pFreeHead == NULL)
			UnlinkAvail(page);          // 이 페이지는 꽉 찼다
		if (page->usedCount++ == 0)
			UnlinkEmpty(page);          // 더 이상 빈 페이지가 아니다
		__p->m_sObj.dwpFlag = page->slot;
		return (T*)&__p->m_sObj.data;
	}

	inline void Confer(T* buf)
	{
		SNodePool* __p = (SNodePool*)((u8*)buf - kDataOffset);
		STPool* page = m_sPageSlots[__p->m_sObj.dwpFlag];   // m_pNext 로 덮이기 전에 읽는다

		if (page->pFreeHead == NULL && page->inAvail == false)
			PushAvail(page);
		__p->m_pNext = page->pFreeHead;
		page->pFreeHead = __p;

		// 여기서 시계를 읽지 않는다. 페이지가 자주 비었다 찼다 하는 워크로드에서
		// Confer 마다 QueryPerformanceCounter 를 부르게 되기 때문이다.
		// 빈 시각은 다음 Collect 가 꼬리 구간을 한 번에 찍는다.
		if (--page->usedCount == 0)
			PushEmpty(page);
	}

	// 할 일이 없으면 시계도 안 보고 O(1) 로 빠진다.
	inline bool HasEmptyPages() const { return m_pEmptyHead != NULL; }

	// 보유 시간을 넘긴 빈 페이지를 앞에서부터 pageBudget 장까지 OS 로 돌려준다.
	// pageBudget 은 해제한 만큼 줄어든다 — 여러 풀이 한 프레임 예산을 나눠 쓴다.
	// 반환값 = 돌려준 바이트.
	size_t Collect(NeoPoolClock::time_point now, int holdMs, int& pageBudget)
	{
		if (m_pEmptyHead == NULL || pageBudget <= 0)
			return 0;

		// 지난 호출 이후 비워진 구간에만 빈 시각을 찍는다(새로 빈 게 없으면 O(1)).
		for (STPool* p = m_pUnstamped; p != NULL; p = p->pNextEmpty)
		{
			p->emptySince = now;
			p->emptyStamped = true;
		}
		m_pUnstamped = NULL;

		size_t freed = 0;
		while (m_pEmptyHead != NULL && pageBudget > 0)
		{
			STPool* page = m_pEmptyHead;   // 가장 오래 비어 있는 페이지
			const long long emptyMs =
				std::chrono::duration_cast<std::chrono::milliseconds>(now - page->emptySince).count();
			if (emptyMs < holdMs)
				break;                     // 뒤쪽은 더 최근이다 — 볼 필요가 없다

			// 슬롯을 먼저 기록한다. push_back이 던지면 페이지는 아직 어느 체인도
			// 바뀌지 않았으므로 그대로 남는다. 성공 뒤의 unlink/free는 no-throw다.
			m_sFreeSlots.push_back(page->slot);
			UnlinkEmpty(page);
			UnlinkAvail(page);
			ReleasePageData(*page);
			page->pData = NULL;
			page->pFreeHead = NULL;
			freed += sizeof(SNodePool) * (size_t)m_iBlkSize;
			m_nReservedBytes -= sizeof(SNodePool) * (size_t)m_iBlkSize;
			--pageBudget;
		}
		return freed;
	}

	inline size_t ReservedBytes() const { return m_nReservedBytes; }
};


template <typename T, int iBlkSize = 128>
class CNVMInstPool
{
	struct STNode
	{
		u32 dwpFlag;            // 사용 중일 때 = 소속 페이지 슬롯 번호
		// T를 멤버로 두면 T가 비표준 레이아웃인 경우 STNode/SNodePool도 비표준
		// 레이아웃이 되어 offsetof가 조건부 지원이 된다. raw storage로 두고 아래
		// GetData에서 객체를 복원하면 노드 헤더의 레이아웃은 항상 표준이다.
		alignas(T) u8 data[sizeof(T)];
	};

	// 생성자/소멸자를 살려야 해서 union 이 아니라 별도 필드다.
	struct SNodePool
	{
		SNodePool*	m_pNext;
		STNode		m_sObj;
	};

	struct STPool
	{
		SNodePool*	pData;          // NULL = 데이터가 해제된 헤더(슬롯 재사용 대기)
		SNodePool*	pFreeHead;
		STPool*		pPrevAvail;
		STPool*		pNextAvail;
		STPool*		pPrevEmpty;
		STPool*		pNextEmpty;
		int			usedCount;
		u32			slot;
		bool		inAvail;
		bool		emptyStamped;   // 빈 시각을 찍었는가(Confer 는 안 찍는다)
		NeoPoolClock::time_point emptySince;
	};

	std::deque<STPool>		m_sPages;
	std::vector<STPool*>	m_sPageSlots;
	std::vector<u32>		m_sFreeSlots;
	STPool*					m_pAvailHead = NULL;
	STPool*					m_pEmptyHead = NULL;
	STPool*					m_pEmptyTail = NULL;
	STPool*					m_pUnstamped = NULL;

	// CNVMAllocPool 과 동일한 의미의 "확보 용량". 노드 안 T 의 멤버가 따로 힙을 잡는 경우
	// (예: CoroutineInfo 의 var 스택 vector) 는 여기 포함되지 않는다 — 소유자가 따로 센다.
	size_t m_nReservedBytes = 0;

	static T* GetData(SNodePool* node)
	{
		// 객체는 페이지 생성 시 이 정확한 주소에 placement new로 만들어진다.
		// std::launder는 C++17부터라, 구형 Visual Studio 프로젝트도 빌드할 수 있게
		// 여기서는 새 포인터를 직접 복원한다.
		return reinterpret_cast<T*>(node->m_sObj.data);
	}

	// 소멸자가 돌아 노드 안 std::string 등도 같이 해제된다.
	// 페이지당 노드 수만큼 소멸자가 돌기 때문에, 이 호출을 프레임당 몇 장으로 제한하는
	// 것이 증분 회수의 핵심이다(Collect 의 pageBudget).
	void ReleasePageData(STPool& page)
	{
		for (int i = 0; i < m_iBlkSize; ++i)
			GetData(&page.pData[i])->~T();
		delete [] page.pData;
	}

	// --- 체인 조작 ---
	NEOPOOL_COLD void PushEmpty(STPool* page)
	{
		page->pPrevEmpty = m_pEmptyTail;
		page->pNextEmpty = NULL;
		if (m_pEmptyTail != NULL)
			m_pEmptyTail->pNextEmpty = page;
		else
			m_pEmptyHead = page;
		m_pEmptyTail = page;

		page->emptyStamped = false;
		if (m_pUnstamped == NULL)
			m_pUnstamped = page;
	}

	NEOPOOL_COLD void UnlinkEmpty(STPool* page)
	{
		if (m_pUnstamped == page)
			m_pUnstamped = page->pNextEmpty;
		if (page->pPrevEmpty != NULL)
			page->pPrevEmpty->pNextEmpty = page->pNextEmpty;
		else
			m_pEmptyHead = page->pNextEmpty;
		if (page->pNextEmpty != NULL)
			page->pNextEmpty->pPrevEmpty = page->pPrevEmpty;
		else
			m_pEmptyTail = page->pPrevEmpty;
	}

	NEOPOOL_COLD void PushAvail(STPool* page)
	{
		page->pPrevAvail = NULL;
		page->pNextAvail = m_pAvailHead;
		if (m_pAvailHead != NULL)
			m_pAvailHead->pPrevAvail = page;
		m_pAvailHead = page;
		page->inAvail = true;
	}

	NEOPOOL_COLD void UnlinkAvail(STPool* page)
	{
		if (page->inAvail == false)
			return;
		if (page->pPrevAvail != NULL)
			page->pPrevAvail->pNextAvail = page->pNextAvail;
		else
			m_pAvailHead = page->pNextAvail;
		if (page->pNextAvail != NULL)
			page->pNextAvail->pPrevAvail = page->pPrevAvail;
		page->inAvail = false;
	}

	void clear()
	{
		for (auto it = m_sPages.begin(); it != m_sPages.end(); ++it)
			if ((*it).pData != NULL)
				ReleasePageData(*it);
		m_sPages.clear();
		m_sPageSlots.clear();
		m_sFreeSlots.clear();
		m_pAvailHead = NULL;
		m_pEmptyHead = NULL;
		m_pEmptyTail = NULL;
		m_pUnstamped = NULL;
		m_nReservedBytes = 0;
	}

	// raw SNodePool 배열은 T를 자동 생성하지 않는다. placement new가 중간에
	// 던져도 이미 생성된 T만 정확히 파괴하고 배열을 해제하기 위한 가드다.
	// alloc() 본문은 try/catch 없이 이 가드의 스코프 소멸만으로 정리한다.
	struct InstDataGuard
	{
		SNodePool* pData;
		int constructed = 0;

		explicit InstDataGuard(int count) : pData(new SNodePool[count]) {}
		~InstDataGuard()
		{
			for (int i = constructed - 1; i >= 0; --i)
				GetData(&pData[i])->~T();
			delete [] pData;
		}
		InstDataGuard(const InstDataGuard&) = delete;
		InstDataGuard& operator=(const InstDataGuard&) = delete;

		void ConstructAll(int count)
		{
			for (; constructed < count; ++constructed)
				::new (static_cast<void*>(pData[constructed].m_sObj.data)) T();
		}

		SNodePool* release()
		{
			SNodePool* result = pData;
			pData = NULL;
			constructed = 0; // 이후 페이지가 모든 T의 소멸을 책임진다.
			return result;
		}
	};

	NEOPOOL_COLD void alloc()
	{
		// CNVMAllocPool::alloc 과 같은 순서 규칙 — 데이터를 먼저, 장부를 나중에.
		// new[] 는 스스로 던지므로 null 검사는 필요 없지만, 던진 시점에 장부를 이미
		// 건드렸다면 재사용 슬롯이 free 목록에서 영구히 사라진다. 순서를 뒤집어 그
		// 문제 자체를 없앤다(try/catch 불필요).
		InstDataGuard data(m_iBlkSize);
		data.ConstructAll(m_iBlkSize);

		STPool* page;
		if (m_sFreeSlots.empty())
		{
			// reserve가 실패하면 data 가드가 완성된 T와 raw 배열을 함께 정리한다.
			// 이후 push_back은 포인터 한 개의 no-throw 삽입이다.
			m_sPageSlots.reserve(m_sPageSlots.size() + 1);
			m_sPages.push_back(STPool());
			page = &m_sPages.back();
			page->slot = (u32)m_sPageSlots.size();
			m_sPageSlots.push_back(page);
		}
		else
		{
			const u32 slot = m_sFreeSlots.back();
			page = m_sPageSlots[slot];
			page->slot = slot;
			m_sFreeSlots.pop_back();     // 던질 일이 끝난 뒤에 슬롯을 소비한다
		}

		// 여기서부터는 던지는 연산이 없다. 모든 T가 생성됐으므로 이제 페이지로
		// 소유권을 넘겨 이후 Collect/clear가 전체 소멸을 담당하게 한다.
		page->pData = data.release();
		page->usedCount = 0;

		SNodePool* pData = page->pData;
		for (int i = m_iBlkSize - 2; i >= 0; i--)
			pData[i].m_pNext = &pData[i + 1];
		pData[m_iBlkSize - 1].m_pNext = NULL;
		page->pFreeHead = &pData[0];

		page->inAvail = false;
		PushAvail(page);
		PushEmpty(page);

		m_nReservedBytes += sizeof(SNodePool) * (size_t)m_iBlkSize;
	}

public:
	int m_iBlkSize = 1;
	u32 _dwLastID = 0;

	CNVMInstPool() { m_iBlkSize = iBlkSize; }
	virtual ~CNVMInstPool() { clear(); }

	// raw storage 기반이므로 offsetof의 두 대상은 표준 레이아웃이다.
	static_assert(std::is_standard_layout<STNode>::value && std::is_standard_layout<SNodePool>::value,
		"pool node headers must be standard-layout");
	static const size_t kDataOffset = offsetof(SNodePool, m_sObj) + offsetof(STNode, data);
	static_assert(kDataOffset % alignof(T) == 0,
		"pool node payload offset must satisfy alignof(T)");

	inline T* Receive()
	{
		if (m_pAvailHead == NULL)
			alloc();

		STPool* page = m_pAvailHead;
		SNodePool* __p = page->pFreeHead;
		page->pFreeHead = __p->m_pNext;
		if (page->pFreeHead == NULL)
			UnlinkAvail(page);
		if (page->usedCount++ == 0)
			UnlinkEmpty(page);
		__p->m_sObj.dwpFlag = page->slot;
		__p->m_pNext = NULL;
		return GetData(__p);
	}

	inline void Confer(T* buf)
	{
		SNodePool* __p = (SNodePool*)((u8*)buf - kDataOffset);
		STPool* page = m_sPageSlots[__p->m_sObj.dwpFlag];

		if (page->pFreeHead == NULL && page->inAvail == false)
			PushAvail(page);
		__p->m_pNext = page->pFreeHead;
		page->pFreeHead = __p;

		// 여기서 시계를 읽지 않는다(반납 핫패스). 빈 시각은 다음 Collect 가 찍는다.
		if (--page->usedCount == 0)
			PushEmpty(page);
	}

	// 할 일이 없으면 시계도 안 보고 O(1) 로 빠진다.
	inline bool HasEmptyPages() const { return m_pEmptyHead != NULL; }

	// 보유 시간을 넘긴 빈 페이지를 앞에서부터 pageBudget 장까지 OS 로 돌려준다.
	// pageBudget 은 해제한 만큼 줄어든다. 반환값 = 돌려준 바이트.
	size_t Collect(NeoPoolClock::time_point now, int holdMs, int& pageBudget)
	{
		if (m_pEmptyHead == NULL || pageBudget <= 0)
			return 0;

		for (STPool* p = m_pUnstamped; p != NULL; p = p->pNextEmpty)
		{
			p->emptySince = now;
			p->emptyStamped = true;
		}
		m_pUnstamped = NULL;

		size_t freed = 0;
		while (m_pEmptyHead != NULL && pageBudget > 0)
		{
			STPool* page = m_pEmptyHead;
			const long long emptyMs =
				std::chrono::duration_cast<std::chrono::milliseconds>(now - page->emptySince).count();
			if (emptyMs < holdMs)
				break;

			// 슬롯 기록이 실패하면 페이지는 아직 intact다. 성공 뒤에는 페이지
			// unlink와 T 소멸만 남고, 이 경로에서는 더 이상 할당하지 않는다.
			m_sFreeSlots.push_back(page->slot);
			UnlinkEmpty(page);
			UnlinkAvail(page);
			ReleasePageData(*page);
			page->pData = NULL;
			page->pFreeHead = NULL;
			freed += sizeof(SNodePool) * (size_t)m_iBlkSize;
			m_nReservedBytes -= sizeof(SNodePool) * (size_t)m_iBlkSize;
			--pageBudget;
		}
		return freed;
	}

	inline size_t ReservedBytes() const { return m_nReservedBytes; }

	// 반납되어 free 리스트에 올라와 있는 노드를 훑는다.
	// StringInfo 처럼 노드가 std::string 같은 부속 힙을 들고 있는 타입에서,
	// "지금 놀고 있는 노드가 얼마나 붙들고 있는가" 를 재는 데 쓴다.
	// 데이터가 해제된 헤더는 pFreeHead 가 NULL 이라 자연히 건너뛴다.
	template <typename F>
	void ForEachFree(F fn)
	{
		for (auto it = m_sPages.begin(); it != m_sPages.end(); ++it)
			for (SNodePool* n = (*it).pFreeHead; n != NULL; n = n->m_pNext)
				fn(*GetData(n));
	}
};



};

#undef NEOPOOL_COLD
