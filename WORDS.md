# Forth Built-In Words

Here you will find a list of the built-in words that are part of the "forthduino" project, along with selected discussions of the Forth language and its implementation here. This list of words includes the expected primitives, control structures, and so on. It also includes words that support the specific WS2812 interfaces and other Arduino-specific capabilities.

## What Are Words?

In Forth, symbols that are bound to a variable or a sequence of instructions (corresponding to functions) are called 'words'. The language consists of a continuous series of whitespace-separated values (literals) and words. A Forth parser need only break up an input string on 'space' and process each of the resulting tokens. If a token matches a defined word, then process the bound value of that word. Otherwise it must be a literal.

## Execution Stack

Forth has a single execution stack where computations are carried out. When a literal value is encountered, it is pushed onto the execution stack. When a built-in word is encountered, it is called. The code for any word expects arguments on the execution stack, which it pops off as needed, and it is expected to push any result back onto the stack.

This approach means that words cannot vary their behaviour based on the number of inputs, unless a convention is adopted of pushing the number of arguments as the last argument. For example, a theoretical 'average' word could accept any number of numeric arguments followed by a count of the number of arguments. It could then pop the count, sum that count of arguments, divide, and push the result.

In this implementation, I have avoided this convention and generally stuck to fixed arguments for all built-in words. There is nothing preventing the introduction of new words that process variable numbers of arguments in this way. Also read below about arrays and how they are processed.

## Data Types

A minimal Forth implementation is typically very tiny and only operates on integer values. This can be traced back to the first implementations which were designed for very small computing architectures where the interpreter was kept as small and simple as possible.

This implementation is a little richer, and has the following built-in data types:

	int
	float
	string
	int array
	sequence

There are additional data types for symbol, function, and free list management, but they are not really directly visible.

An int is any integer. A float is a number with a decimal place. A string is a sequence of characters prefixed by a single quote ' sigil. Forth uses white space as a word delimiter, so strings cannot contain white space, and there is no support for strings that would contain white space (this could be introduced later). This Forth is designed to manipulate numeric values into colors to animate RGB LED strings, so string processing is not really a core requirement. (note: the alpha LED message board functions introduce this need so more complete string processing might be introduced soon)

An int array is a fixed array of integers. These arrays can be used to hold color values (for example hue, saturation, and value; or red, green, blue; or packed RGB values). This would typically be used to compute whole arrays of color values matching a whole LED string. Most operators are enhanced to process arrays by applying themselves to each element of the input arrays in turn, so this kind of color processing can be very fast and easy.

Finally, a sequence is a bunch of words representing either the body of a function or a code fragment that is to be executed conditionally or repeatedly. A sequence can be declared as follows:

```
[ 5 + . ] 0 10 loop cr
```

This defines a sequence that adds 5 to an input argument, and then prints it. The sequence is executed 10 times by 'loop', with arguments 0..9 in turn.

## Stash Stack

Almost all computation happens on the execution stack. A parallel stack called the 'stash' stack can also be used to temporarily hold values. This can sometimes be valuable in avoiding some complex swapping and rotating of the execution stack.

For example, the following word definition takes the product of three separate sums: six input arguments, one value returned.

```
:product-sum-3
	+ >>> + >>> + <<< * <<< * ;
```

Note that '>>>' moves the top of execution stack to the stash stack, and '<<<' does the reverse.

## Global Dictionary

Variables and defined words are held in a single global dictionary. There is no concept of other scopes, no binding of arguments to a function activation scope, etc., just a single global scope where everything is defined.

This is part of the extreme simplicity of the language design. It does mean that there is a risk of collisions in the global dictionary, so the prefix convention below is used to manage this somewhat.

Words can be defined (`def`) and forgotten (`forget`).

## Prefix Convention

Words can contain almost any non-whitespace character. A colon ':' character is conventionally used to designate a prefix or namespace to keep related words organized, and to disambiguate similar or identical words that operator on different data types.

For example, the prefix "pin:" is used to organize all of the words that manipulate the GPIO pins.

## Unu Comment Format

Ok, I can't find a reference to this commenting convention now, but I think it was called "unu". Forth files that are read from the SD card are assumed to be in this format, which allows alternating comment and code sections. The document opens in 'comment' mode, ignoring lines until a line is encountered that contatins three tildes ~~~ in the first three columns. This is a signal to switch to the other mode.

Each Forth file will therefore start with introductory comments, switch to some code, switch back to comments to introduce the next block of code, and so on.

example:

```
This file contains supporting code for LED appliances
attached to the OctoWS2812 interface. This should be modified
later so the octo: controls can be swapped out for other
configurations of LED string.

This file contains no specific animations that are based on
the arrangement of LEDs for a given display.


mem:prt  Print memory stats to serial
~~~
:memprt (-)
	'memstats: .
	mem:malloc .
	mem:alloc .
	mem:free .
	mem:calloc .
	mem:cfree .
	mem:amalloc .
	mem:afree .
	cr ;
~~~


Initialize an array 'rainbow with low intensity rainbow colors 0..359
~~~
:init-rainbow (-)
	360 array identity 100 5 hsvr>
	!rainbow ;
~~~

and so on...
```

## Sigils
A sigil is a single character prefix on a word that applies some action to the rest of the word. For example, the ! sigil defines a word in the global dictionary.

`100 !foo` --> defines the word `foo` and assigns the value `100`

`!word` defines `word` with the next value on the stack

`@word` recalls the value of `word` and pushes its value onto the stack

`#hex` pushes an int represented by the hexadedimal string `hex`

`'string` pushes a string literal containing the characters of `string`

`:word` starts a sequence definition where the sequence that follows will be defined globally as `word`. The sequence is terminated with the `;` sigil.

`(anything` defines a comment. The entire word, terminated by whitespace, is ignored. This is typically used to define an input / output command in a sequence definition.

```
:num:dadd (ii-d)
	+ 0.0 + ;
```

The sequence `[ + 0.0 + ]` is defined with the word `num:dadd`. The comment indicates that two integer inputs are expected, and a single double output is returned.

## Defining Words

As you can see in the above and previous examples, new words can be defined in a couple of ways. The nicest is the colon shortcut used above. The colon sigil starts the definition of a new sequence, that sequence consisting of every word that follows until the ';' semicolon word is encountered to terminate the sequence definition. This is a shortcut to calling the existing words to declare and bind a sequence.

```
:thing1
	* rot * + sqrt ;

is equivalent to:

[ * rot * + sqrt ] 'thing1 def

but the first form seems more readable, and looks something like a function definition in other languages.
```

Note that the : sigil places the symbol to be bound at the beginning of the form, which is counter to everything else about the Forth language, as the implementation of that sigil must remember the word being defined and then use that word when the terminating semicolon is found. Thus function definitions are not reentrant, but that seems reasonable given the extremely simple structure of the language and data structures.

## Math Primitives

### + - * / mod
These are unprefixed binary operators that expect two arguments and return one.

`1 2 + 3 +` --> `6`

### sq
Square a numeric input.

`5 sq` --> `25`

### sqrt
Return the square root of a numeric input.

`25 sq` --> `5.0`

### constrain
Constrain a numeric input to an inclusive range.

`n 50 100 constrain` --> `n, or 50 if n < 50, or 100 if n > 100`

### sin cos tan asin acos atan

Return corresponding trig operators. Inputs and outputs are in Radians.

### deg

Convert an input in radians to degrees.

### rad

Convert an input in degrees to radians

### pow

Raise X to the Yth power.

`2 5 pow` --> `32`

### abs

Return the absolute value of an input.

### min max

Return the min or max of a pair of inputs.

### round ceil floor

Return the rounded value, ceiling, or floor, of a single numeric input.

## Stack Manipulations

### dup

Duplicate the top-of-stack.

### over

Duplicate the value one below the top-of-stack

`1 2 3 over` --> `1 2 3 2`

### aty atz atu atv atw

Duplicate a value up to six positions below the top-of-stack. This uses the convention of labeling the top-of-stack as `x`, the next value down as `y`, and so on using letters `z`, `u`, `v`, and `w` for the next positions down in order.

### at

Duplicate a value an arbitrary number of positions down in the stack. The top-of-stack is labelled as position `0` and each value down incrementally.

`1 2 3 4 2 at` --> `1 2 3 4 2`

### swap

Swaps the two top values on the stack.

### rot

Rotates the top three values in the stack such that the previous top-of-stack is now third.

### rup

Rotates the top three values on the stack in reverse of `rot`, such that the previously third item is now top.

### rot4

Same as `rot`, but applies to the top four items.

### rup4

Same as `rup`, but applies to the top four items.

### rotn

Same as `rot`, but takes an additional argument specifying how many stack elements to rotate.

### rupn

Same as `rup`, but takes an additional argument specifying how many stack elements to rotate.

### drop

Drop the top-of-stack.

### dup2

Duplicate the top two elements of the stack.

### drop2

Drop the top two elements of the stack.

### clst

Clear the entire evaluation stack.

### >>>

Pop the top element of the evaluation stack, and push it onto the stash stack.

### <<<

Pop the top element of the stash stack, and push it onto the evaluation stack.

### < swap >

Swap the entire contents of the evaluation stack and the stash stack.

## Array of Integer operations

Arrays of integer are a basic data type because manipulating color values for an array of RGB LEDs is one of the basic objectives for the language. Arrays of int are created with `array` and then can be manipulated directly using any basic operator such as + - * / and so on.

Most numeric operators will check if their arguments are simple integer values or arrays, and will apply the operation across the entire array.

`5 array identity 5 *` --> `(0,5,10,15,20)`

`20 +` --> `(20,25,30,35,40)`

`37 gt` --> `(0,0,0,0,1)`


### sum

Return the sum of an array.

### array

Create and push a new array of int.

### identity

Populate the array at TOS with an identity matrix.

`10 array identity` --> `(0,1,2,3,4,5,6,7,8,9)`

### geta puta

Get or Put an element of an int array. The array is on the stack, and following the operation the array remains on the stack. The other arguments are consumed normally.

`5 array identity 2 100 puta` --> `(0,1,100,3,4)`

`5 array identity 5 * 1 geta` --> `5 (0,5,10,15,20)`

### dgeta dputa

Get or Put an element of an int array, where the array is bound to a symbol in the global dictionary.

`5 array !mydata`

`'mydata 0 100 dputa` --> `mydata:(100,0,0,0,0)`

`'mydata 0 dgeta` --> `100`

### size

Return the size of an array. Does not consume the array from the stack.

### map

Execute a sequence for each element of an array, returning the result into the array.

`5 array identity` --> `(0,1,2,3,4)`

`[ 5 * 7 + ] map` --> `(7,12,17,22,27)`

## Inequalities

Inequalities are all binary operators that taken two values from the stack, compare them, and return a boolean result to the stack as int 0 or 1.

### eq

Equals:

`5 2 3 + eq` --> `1`

### ne

Not equals:

`5 2 3 + ne` --> `0`

### gt

Greater than:

`5 2 2 + gt` --> `1`

### lt

Less than:

`5 2 2 - lt` --> `0`

### ge

Greater than or equal:

`5 2 2 + ge` --> `1`

### le

Less than or equal:

`5 2 2 + le` --> `0`

## Boolean Operations

Booleans are integers with value 0 (false) or non-zero (true).

### and

`1 1 and` --> `1`

`1 0 and` --> `0`

### or

`1 1 or` --> `1`

`1 0 or` --> `1`

`0 0 or` --> `0`

### not

`1 not` --> `0`

`0 not` --> `1`

## Conditionals and Looping

Conditionals and looping use sequences to represent the conditional or repeated section of code. The sequence is pushed onto the stack and consumed by the operator.

### if

Conditionally execute a sequence if the tos is boolean truthy.

`17 [ 1 - ] over 15 gt if` --> `16`

`12 [ 1 - ] over 15 gt if` --> `12`

### ife

Conditionally execute one sequence or another based on the truthiness of the tos.

`17 [ 1 - ] [ 1 + ] atz 15 gt ife` --> `16`

`12 [ 1 - ] [ 1 + ] atz 15 gt ife` --> `13`

### loop

Execute a sequence for integers in a range.

```
[ 5 * . ] 0 5 loop cr
prints> 0 5 10 15 20
```

### repeat

Repeat a sequence a given number of times. No arguments are passed into the sequence.

```
[ 'hello . ] 5 repeat cr
prints> hello hello hello hello hello
```

### call

## RGB Colors

Three color models are supported so far: RGB, HSV, and HSV with a rainbow modifier to produce smoother yellows. I prefer the last, because the colors and saturation are the most pleasing, and the HSV model is so much more intuitive to work with for producing animations.

All of the color support is oriented to WS2812 strips and similar products. The Octo library is used to render color values out to eight strings of LEDs simultaneously. The eight strings of equal length are attached to eight GPIO ports with TTL logic level conversion. The Octo library presents the eight strings as a single continuously addressable string.

For example, the MAP project consists of 2208 LEDs in eight strings of 276 LEDs each. Each string of 276 LEDs is formed of alternating strings of 35 LEDs (left-to-right) and 34 LEDs (right-to-left, on the offset). Four such pairs are connected in series to make a single strand, and eight strands make up the map, altogether approximately 35 LEDs wide and 64 rows tall.

This complicated arrangement of LEDs is wired to the Octo interface, and is then addressable as a flat array of 2208 pixels.

The map has a supporting library (in Forth) that supports the addressing of pixels by an (x,y) coordinate system, and by (lat,long). For example, here is the Forth function that converts (lat,long) into (x,y) coordinates for the specific canvas North America map used in the project:

```
:map:xy (lat,long-x,y)
	-95 - rad sin !map:sinlong 45 - rad sin !map:sinlat
	1.419 @map:sinlat *
	53.725 @map:sinlong * +
	-0.583 @map:sinlat sq * +
	0.295 @map:sinlong sq * +
	-56.426 @map:sinlat * @map:sinlong * +
	33.565 +
	round
	-65.793 @map:sinlat *
	-0.26 @map:sinlong * +
	2.058 @map:sinlat sq * +
	-17.021 @map:sinlong sq * +
	-0.808 @map:sinlat * @map:sinlong * +
	32.917 +
	round ;
```

Note that the first line of code takes the sin of the lat and long input values, and stores them in `map:` prefixed variables for use in the following two polynomials.

Colors in rgb or hsv format must be converted to a packed rgb format before being send to the Octo library or other libraries that manage the serial output of color data. The ordering of the red, green, and blue components may vary based on the hardware. This ordering is set with the `rgbformat` word.

### rgbformat

Sets the format for packing RGB color values into an int.

0: RGB
1: GRB
2: BGR
3: GBR
4: RBG
5: BRG

### rgb>

Convert r, g, b values into a packed int.

### >rgb

Unpack an int into its r, g, b components.

### hsv>

Convert h, s, v values into rgb and then a packed int. Hues are not corrected, so yellow occupies a narrow band and blue is quite wide.

### hsvr>

Convert h, s, v values into a packed rgb int. Hues are corrected so each rainbow member occupies roughly an equal portion of the hue space.

### blend

Mixes two packed RGB values using a ratio.

`a b 75 blend` --> results in new color that is 25% a and 75% b.

### ablend

Same as blend, but takes two arrays as input and mixes each corresponding pixel.

## Dictionary Definitions

### def

Define a word. May be used to define a function or a variable.

`[ sq swap sq + sqrt ] 'pyth def` --> defines a new word that applies Pythagorean theorem.

`100 'a def` --> defines a new variable `a` bound to the value 100.

### redef

Redefines a word without removing the previous definition. The global dictionary can have multiple definitions of the same word, and only the most recent will be visible. When a rederinition is removed with `forget`, the previous definition will become visible again.

### forget

Remove the most recent definition of a word from the global dictionary.

### def?

Return 1 if a word is defined in the global dictionary.

### vget

Return the value bound to a word in the global dictionary.

## Debugger

### step

Turn on/off step mode. Takes a single int argument.

### rb

Force a reboot of the device.

## Memory Status

### mem:malloc mem:alloc mem:free mem:calloc mem:cfree mem:amalloc mem:afree mem:sram

Return various memory metrics;

malloc: number of values allocated with malloc since start
alloc: number of values allocated with malloc or from the free list
free: number of values freed (returned to free list)
calloc: number of values currently allocated
cfree: number of values in the free list
amalloc: number of int arrays allocated
afree: number of in arrays freed
sram: approximation of available RAM

## Other

### stack:size

Return the number of elements on the evaluation stack.

### num:dec

Convert a float to a string with a given format (width and decimal places).

`54.855 5 1 num:dec` --> `" 54.9"`

### num:sci

Convert a float to a string using scientific notation with a given format (width and decimal places).

`54.855 5 1 num:sci` --> `"5.5E+01"`

### str:mid

Return a substring from a string.

`'ThisIsAString 4 2 str:mid` --> `"Is"`

### rndm

Return a random int between 0 (inclusive) and n (exclusive).

`[ 3 rndm ] 10 repeat prtstk`

prints> 1 2 1 1 0 1 1 2 0 0 (typical)

### rrndm

Return a random int between lower (inclusive) and upper (exclusive).

`[ 1 3 rrndm ] 20 repeat prtstk`

prints> 2 2 2 1 2 2 1 2 2 1 1 1 2 2 1 1 1 1 1 1 (typical)

### delay

Delay the given number of milliseconds.

### delayus

Delay the given number of microseconds.

### now

Return the current ticks, which is milliseconds since the device started. The Teensy hardware does not have a real time clock, but ticks can be used to reliably calculate durations.

## Octo WS2812 Controller

The octo library has an internal buffer of color values that can be written on demand to the physical LED devices. One usual practice is to set a bunch of color values with octo:pixel, and then write the buffer to the LEDs with octo:show.

Array operations can also be used to build and compute whole arrays of color values, and then write an array to the Octo library all at once.

### octo:init

Initialize the OctoWS2812 library. Requires two arguments: total LEDs, and LEDs per strip. The octo integration requires preallocated buffers for LED values, which are preset to a maximum of 1100 LEDs per strip (8800 LEDs total).

Initalize the map project:

`2208 276 octo:init`

### octo:showa

Write an array of int directly to the LED array. The values in the array will replace the current color values in the library.

### octo:reada

Read the current LED array into a given int array. This is useful, for example, to animate a transition from whatever the current LED values are to a new set of values.

`2208 array octo:reada`

### octo:pixel

Write a single pixel value to the octo array.

`150 100 10 hsv> 15 octo:pixel` --> write hsv (150,100,10) to pixel 15

### octo:fill

Fill a range of pixels with a color value.

`150 100 10 hsv> 20 50 octo:fill` --> fill pixels 20..49 inclusive with a cyan color

`0 0 100 octo:fill` --> clear first 100 pixels (0..99)

### octo:show

Write the Octo library's array of LED values to the physical LEDs. Note that `octo:pixel` and `octo:fill` and other calls that affect the internal array of color values will not render the array out to the physical LEDs. A separate call to `octo:show` is generally required.

An exception is `octo:showa` which transfers an array of pixel values to the internal array, and then writes the internal array to the LEDs. That call assumes that a complete frame of color values is pre-calculated into an int array, and then copied to the LED strings.

### octo:dma-wait

When the octo library writes its internal array out to the LEDs, it uses memory buffers and DMA (direct memory access) to write the whole pixel array at 800 KHz. This operation takes some time, usually a few milliseconds, but the main processor is not required and control returns immediately to the running process.

When writing a set of frames as an animation, it will produce unknown or unpredictable results if frames are written while the previous frame is still being rendered by the DMA process.

`octo:dma-wait` will wait until the current DMA process is writing data, and will return as soon as the frame is complete. To get a maximum frame rate without overlapping frames, use `octo:dma-wait` just before writing a new frame of data, to make sure the previous frame has completed. This approach will optimize use of the main processor to prepare the next frame, and then render it as soon as possible (but no sooner).

### octo:set-map

Sometimes, the order of pixels in the Octo library may not be an easy or natural pixel ordering for animations. For example, one might want to create a pixel numbering scheme that puts pixel 0 at the bottom-left of a matrix. Another example could be mis-wiring of the eight strings such. Another example might be strings that are laid out vertically in an up-and-down alternating arrangement, where the natural pixel ordering for animations should be consistently left-to-right.

In cases such as these, functions can be written to translate the ideal pixel order for animations into the physical mapping. The function `octo:set-map` takes this a step further by allowing a translation map to be built once at startup and asserted as the pixel translation for `octo:pixel` and other related functions.

Here is an example where the North America map was miswired. It is a lot easier to set a translation map than it is to desolder and correct the wiring error.

```
2208 array
[
	!idx
	@idx @idx 828 + puta
	@idx 276 + dup puta
	@idx 552 + dup puta
	@idx 828 + @idx puta
	@idx 1104 + @idx 1932 + puta
	@idx 1380 + dup puta
	@idx 1656 + dup puta
	@idx 1932 + @idx 1104 + puta
] 0 276 loop
octo:set-map
```

The rows starting at indices 276, 552, 1380, and 1656 are correct and map to themselves. Rows at index 0 and 828 are swapped, and rows at 1104 and 1932 are swapped.

## Octo With 8xN Alpha Matrix

One application of the Octo library is to drive eight matrices of LEDs that are each 8 rows by 32 columns. Stringing these together horizontally produces a single matrix that is 8 rows by 256 columns.

These `alpha:` prefixed functions use a matrix like this to render alphanumeric characters using a 5x7 pixel ASCII character set.

### alpha:at

Render a string starting at a given column and using background and foreground colors.

`'>>hello<< 0 150 100 10 hsv> 0 alpha:at`

Renders ">>hello<<" with cyan foreground and black background, starting at column 0.

### alpha:charat

Render a single character at a given column and using background and foreground colors.

### alpha:charcolat

Write a single column of a character given the base column and offset. This can be used to vary the colors by column for a single character.

### alpha:fontdata

Return the font data for a single column of a given character.

## Serial I/O

These functions operation on the serial interface, treating it as a console.

### .

Print the value at top-of-stack to the serial interface.

### cr

Print a carriage return to the serial interface.

### prtdict

Print the global dictionary to the serial interface.

### prtstk

Non-destructively print the current execution stack to the serial interface.

### cmd:echo

Control the echo of incoming commands to the serial interface. Defaults to on (1). If on, commands received via the serial interface or UDP are echoed to the serial interface before being executed.

## GPIO

### pin:mode

Set the mode of the given pin.

### pin:dread

Execute a digitalRead of the given pin.

### pin:dwrite

Execute a digitalWrite to the given pin.

### pin:aread

Execute an analogRead of the given pin.

### pin:awrite

Execute an analogWrite to the given pin.

## UDP Interactions

Forth code can be submitted to a device over UDP for execution. The network stack on a Teensy 4.1 device is all software, and is configured with `udp:init`. Note that the Teensy will lock up on start if `udp:init` is called, and there is no working EtherNet connection.

### udp:init

Initialize the network and start listening for commands on a UDP port. The MacAddr, IP address, and port are given as arguments. There is no DHCP protocol support so a static IP address must be given. In some cases, it may be desired to map a MacAddr to a static IP address in the router. In this case the MacAddr will be an important parameter.

```
#DE #AD #BE #EF #FE #ED
192 168 1 182
8888
udp:init
```

The first six arguments are the bytes of the MacAddr, here given in hexadecimal using the # sigil. The next four are the buyes of the IP address. Finally, the port to listen on for UDP.

### udp:begin udp:. udp:end

The three words `udp:begin`, `udp:.`, and `udp:end`, are used together to form a reply UDP packet in response to a UDP command having been received. Begin creates a buffer for the response packet and prepares to send that buffer to the IP address and port that originated the most recently received inbound UDP packet.

`udp:.` prints the top value on the execution stack into the UDP packet buffer. This can be called any number of times to accumulate the response string.

`udp:end` sends the packet buffer.

## Arduino Loop

Arduino devices have standard "setup" and "loop" functions. The following calls are implemented in the Arduino loop to allow Forth sequences to be executed on a schedule. This can be used to, for example, render the next frame of an animation.

### loop:def

Defines a sequence to be executed on the Arduino loop.

```
[ [ map:ping-animate ] 20.0 0.0 ] 'maploop loop:def
```

This sets up a call to `map:ping-animate` every 20 milliseconds, maximum. The actual execution may be delayed if the function has a long execution time, or other things are also happening in between scheduled calls.

`loop:def` expects two arguments. The first is a sequence that is a data structure of three elements. The second argument is a string used to name this defined loop behaviour so it can be removed later with `loop:forget`.

The sequence / data structure consists of three elements. The first is another sequence that is the actual target to be executed periodically. The second is the desired minimum delay in milliseconds between calls. The third is a placeholder number that is used internally to hold a tick count for the next scheduled call.

Both of the millisecond values are given as floats, so don't forget the `.0` part. Floats are used because int is usually not wide enough to contain the tick count. It is important to reserve floats in the initial declaration of this structure.

### loop:forget

This removes an existing defined loop sequence. Redefining an existing sequence will implicitly forget the old one.

An idiosyncracy of this Forth is that forgotten loop definitions will leak a small amount of memory, so it is not advisable to constantly define loop sequences. They should be defined once only.

## SD Card

### file:run

## Quad 14 Segment Display

### quad:char

### quad:str

### quad:blank
