# Neo Script Documentation
	- The grammar uses a C-like syntax, but it is somewhat similar to Lua script.
	- It was developed in Visual Studio Pro 2017 C++.
	- After some more features are added, port to C#



### Sample
	- console / callback: Function calls between C ++ and Neo scripts.
	- console / table_callback: maps the function to the Neo script table and finds and calls the function at runtime.
	- console / 9_times: This is a sample of a simple game.
	- console / table: Sample table usage.
	- console / slice_run: Executes every part of the script that is produced by the script in sequential order of a certain time or command.
	- console / time_limit: Processes a script function call to exit after a certain period of time.
		Unlike console / slice_run, console / time_limit does not execute after the next frame.

### var data structure
	- null: the variable has no value (if the variable is not initialized or "var a;" or "var a = null;")
	- bool: store true / false values
	- int: Save 4-byte signed integer value
	- double: 8-byte floating point storage
	- string: store string
	- table: save table (usage is similar to Lua)

	- string / table: managed as a reference for memory savings and performance
	- int / double: "a = 1;" If it is "a = 1.0", double value is stored.
	- If a double operation is performed on an int and a double, it becomes a double value.



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
	- for / while: same as c ++
	- For foreach: talbe variable only. Must be foreach (var a, b in table)
	- true / false: value stored in a variable of type bool
	- null: no value
	- ++ / - Same as c ++ (variable value is incremented by 1 or decremented by 1)
	- && / || : Same as c ++
	- > / < / >= / <= : same as c ++
	- x..y: return the combined string of x and y strings

### Built-in system function (use system.
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
	- rand (): same as c ++

	- str_sub (x, y, z): convert x string from y offset to z number of strings and return
	- str_len (x): Return the length of the string x
	- str_find (x, y): Retrieve y string from x string and return position (same as c ++)
	- print (x): print string x

### Comment
	- Works the same as C syntax
	- // one-line comment
	- / * * / is a multi-line range comment


#### Neo Script Image
![](/docs/img/vs001.png)

