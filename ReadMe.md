# Neo Script Documentation
	- The grammar uses a C-like syntax, but it is somewhat similar to Lua script.
	- It was developed in Visual Studio Pro 2022 C++.
	- After some more features are added, port to C#

### License
	MIT license
	NeoScript is free software.
	The only requirement is that if you do use NeoScript, then you should give us credit by including the copyright notice somewhere in your product or its documentation.

### Sample
	- console / hello: print hello.
	- console / callback: Function calls between C ++ and Neo scripts.
	- console / map_callback: maps the function to the Neo script table and finds and calls the function at runtime.
	- console / 9_times: This is a sample of a Multiplication table.
	- console / string: this is an example of string.
	- console / map: Sample Dictionary usage.
	- console / list: Sample List usage.
	- console / slice_run: Executes every part of the script that is produced by the script in sequential order of a certain time or command.
	- console / time_limit: Processes a script function call to exit after a certain period of time.
		Unlike console / slice_run, console / time_limit does not execute after the next frame.
	- console / divide_by_zero: Exception handling.
	- console / delegate: Function pointer support.
	- console / meta: this is an example of using meta
	- console / coroutine: an example of using coroutines.
	- console / mudule: this is an example of using module.

### var data structure
	- null: the variable has no value (if the variable is not initialized or "var a;" or "var a = null;")
	- bool: store true / false value
	- int: Save 4-byte signed integer value
	- double: 8-byte floating point storage
	- string
	- list
	- map : Dictionary


### Neo Scrit reserved words
	- var: Save the value as a variable name later
	- fun: Notice the start of the function
	- import: prefixed to fun and declared to call a c ++ function
	- export: can be prepended to fun and the function can be called from external (c ++)
	- tostring (x): change the value of x variable to string
	- toint (x): change the value of x variable to int
	- tofloat (x): change the value of x variable to double
	- tosize (x): Returns the number in the table if the variable x is a table, or 0 if it is not a table
	- totype (x): return the data type of the variable x as a string
	- sleep (x): Pause as much as the value of x (1 second for 1000)
	- return [x]: Return x value
	- break: causes for / foreach / while loop to exit
	- if (x) / else: same as c ++
	- for : for (var a int 1, 100, 1)
	- foreach : map, list, set variable only. Must be foreach (var a, b in map)
	- true / false: value stored in a variable of type bool
	- null: no value
	- ++ / - Same as c ++ (variable value is incremented by 1 or decremented by 1)
	- && / || : Same as c ++
	- > / < / >= / <= : same as c ++
	- x..y: return the combined string of x and y strings

### Built-in system function use system.
	## Basic
	- print (x): print string x

	## system
	- clock ():  return time
	- load ():  load module scipt
	- pcall (x) : run mdule script
	- meta(x,y) : bind the y function of var x
	- set(x) : list to set
	
	## coroutine
	- create (): coroutine create and return (suspended mode )
	- resume (x): suspended coroutine active
	- status (x): coroutine state string return
	- close ([x]): coroutine close

	## math
	- abs (x): same as c ++
	- acos (x): same as c ++
	- asin (x): same as c ++
	- atan (x): same as c ++
	- ceil (x): same as c ++
	- floor (x): same as c ++
	- sin (x): same as c ++
	- cos (x): same as c ++
	- tan (x): same as c ++
	- log (x): same as c ++
	- log10 (x): same as c ++
	- pow (x, y): same as c ++
	- deg (x): radian -> degree
	- radian (x): degree -> radian
	- sqrt (x): same as c ++
	- srand (x): same as c ++
	- rand (): same as c ++

    ## string
	- len (): Return the length of the string
	- find (x): Retrieve x string return position (same as c ++)
	- sub (x, y): convert x string  offset to y number of strings and return
	- upper () : convert to uppercase and return
	- lower () : convert to lowercase and return
	- trim () : trim space erase (left, right)
	- ltrim () : trim space erase (left)
	- rtrim () : trim space erase (right)
	- replace (x, y) : find x and change y
	- split (x) : string split from x and return list 

	## list
	- len () : return the length of list item count
	- resize (x) : list item resize
	- append (x, [y]) : list item insert (y value is index)

	## map
	- reserve (x) : allocate x number of buffers.
	- sort () : sort table value
	- keys() : put the keys into a set data structure and return it.
	- values() : put the values into a set data structure and return it.

	## set
	- len () : return count set item

### Comment
	- // one-line comment
	- / * * / is a multi-line range comment


#### Neo Script Code Image
![](/docs/img/vs001.png)

#### Neo Script Output Image
![](/docs/img/vs002.png)


|               | Neo Script     | Lua Script 5.42   |
| :-----------  |:--------------:| -------------:|
| Loop          | Data 2         | Data 3        |
| Math          | Data 5         | Data 6        |
| Math          | Data 5         | Data 6        |