using System;
using System.Collections.Generic;
using System.Net.Http;
using System.Text;
using System.Threading;

namespace NeoScript
{
    /// <summary>전송 콜백의 요청 복사본. 취소 상태는 콜백이 끝나기 전까지만 읽는다.</summary>
    public sealed class NeoHttpRequest
    {
        private readonly object _gate = new object();
        private IntPtr _native;
        public string Method { get; }
        public string Url { get; }
        public string Body { get; }
        public int TimeoutMs { get; }
        public IReadOnlyList<KeyValuePair<string, string>> Headers { get; }

        internal NeoHttpRequest(IntPtr request)
        {
            _native = request;
            Method = Text(NeoNative.NsHttpMethod(request));
            Url = Text(NeoNative.NsHttpUrl(request));
            Body = Text(NeoNative.NsHttpBody(request));
            TimeoutMs = NeoNative.NsHttpTimeout(request);
            var headers = new List<KeyValuePair<string, string>>();
            for (int i = 0; i < NeoNative.NsHttpHeaderCount(request); ++i)
                if (NeoNative.NsHttpHeader(request, i, out var name, out var value) != 0)
                    headers.Add(new KeyValuePair<string, string>(Text(name), Text(value)));
            Headers = headers.AsReadOnly();
        }

        private static string Text(NsStr value) => NeoUtf8.ToString(value.data, value.len);
        public bool IsCancellationRequested
        {
            get
            {
                lock (_gate)
                {
                    if (_native == IntPtr.Zero) throw new ObjectDisposedException(nameof(NeoHttpRequest));
                    return NeoNative.NsHttpIsCancelled(_native) != 0;
                }
            }
        }
        internal void Close() { lock (_gate) _native = IntPtr.Zero; }
    }

    public readonly struct NeoHttpResponse
    {
        public bool Success { get; }
        public string Text { get; }
        public NeoHttpResponse(bool success, string text) { Success = success; Text = text; }
    }

    /// <summary>작업 스레드용 HTTP/HTTPS 전송. 사용하는 런타임을 모두 폐기한 뒤 Dispose한다.</summary>
    public sealed class NeoHttpClientTransport : IDisposable
    {
        private readonly HttpClient _client = new HttpClient(new HttpClientHandler { AllowAutoRedirect = false })
        {
            Timeout = System.Threading.Timeout.InfiniteTimeSpan,
            MaxResponseContentBufferSize = 8 * 1024 * 1024
        };

        public NeoHttpResponse Send(NeoHttpRequest request)
        {
            try
            {
                var uri = new Uri(request.Url);
                if (uri.Scheme != "http" && uri.Scheme != "https") throw new ArgumentException("Unsupported HTTP scheme");
                using (var cancel = new CancellationTokenSource())
                using (var message = new HttpRequestMessage(new HttpMethod(request.Method), uri))
                {
                    if (request.IsCancellationRequested) return new NeoHttpResponse(false, "HTTP request cancelled");
                    if (request.TimeoutMs < -1) throw new ArgumentException("Invalid HTTP timeout");
                    if (request.TimeoutMs >= 0) cancel.CancelAfter(Math.Max(1, request.TimeoutMs));
                    if (request.Method == "POST") message.Content = new ByteArrayContent(Encoding.UTF8.GetBytes(request.Body));
                    foreach (var header in request.Headers)
                        if (!message.Headers.TryAddWithoutValidation(header.Key, header.Value))
                            if (message.Content == null || !message.Content.Headers.TryAddWithoutValidation(header.Key, header.Value))
                                throw new ArgumentException("Invalid HTTP header");
                    var pending = _client.SendAsync(message, HttpCompletionOption.ResponseContentRead, cancel.Token);
                    while (!pending.IsCompleted)
                    {
                        if (request.IsCancellationRequested) cancel.Cancel();
                        Thread.Sleep(10);
                    }
                    using (var response = pending.GetAwaiter().GetResult())
                    {
                        var bytes = response.Content.ReadAsByteArrayAsync().GetAwaiter().GetResult();
                        return new NeoHttpResponse(true, Encoding.UTF8.GetString(bytes));
                    }
                }
            }
            catch (Exception error) { return new NeoHttpResponse(false, error.Message); }
        }

        public void Dispose() { _client.Dispose(); }
    }
}
