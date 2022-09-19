# Forthduino

Forthduino is a project conceived as an intelligent controller of WS2812 and similar LEDs, running on the very capable Teensy 4.1. The excellent FastLED library is used to carry out color management and LED control. This allows a variety of LED types to be attached to individual pins, or to be controlled in parallel using the OctoWS2812 board. This last combination is easy to connect to large numbers of LEDs, it renders at 8x by driving eight sections of LEDs simultaneously, and it presents a simple linear array of color values.

The Teensy 4.1 also has a good amount of RAM and a fast processor, which allows complex renderings to be created at a high frame rate. This library could easily be adapted to other Arduino devices and other LED driving strategies, but the combination described is excellent and easy to work with.

One last feature of the Teensy 4.1 is onboard EtherNet using an inexpensive additional component. All of the components needed are available for order at www.pjrc.com.

In this document you will find a list of the built-in words that are part of the "forthduino" project, along with selected discussions of the Forth language and its implementation here. This list of words includes the expected primitives, control structures, and so on. It also includes words that support the specific LED interfaces and other Arduino-specific capabilities, making this an excellent basis for network-attached sensors and displays.

## What Are Words?

In Forth, words are symbols that are bound to a piece of C code, a variable, or a sequence of instructions (corresponding to functions). The language consists of a continuous series of whitespace-separated words. A Forth parser need only break up an input string on whitespace and process each of the resulting words. If an input word matches a bound word, then process the bound value of that word. Otherwise it must be a literal.

## Execution Stack

Forth has a single execution stack where computations are carried out. When a literal value is encountered, it is pushed onto the execution stack. When a built-in word is encountered, it's C implementation is called. The code for any word expects arguments on the execution stack, which it pops off as needed, and it is expected to push any result back onto the stack.

This approach means that words cannot vary their behaviour based on the number of inputs, unless a convention is adopted of pushing the number of arguments as the last argument. For example, a theoretical 'average' word could accept any number of numeric arguments followed by a count of the number of arguments. It could then pop the count, sum that count of arguments, divide, and push the result.

In this implementation, I have avoided this convention and generally stuck to fixed arguments for all built-in words. There is nothing preventing the introduction of new words that process variable numbers of arguments in this way.

In LED functions such as calculating color values, value is found in applying an algorithm to each LED in a string. For this reason, arrays of integer and implicit array behaviour was introduced.

## Data Types

A minimal Forth implementation is typically very tiny and only operates on integer values. This can be traced back to the first implementations which were designed for very small computing architectures where the interpreter was kept as small and simple as possible.

This implementation is a little richer, and has the following built-in data types:

* int
* float
* string
* int array
* sequence

There are additional internal data types for symbol, function, and free list management.

An int is any integer. A float is a number with a decimal place. A string is a sequence of characters prefixed by a single quote ' sigil. Forth uses white space as a word delimiter, so strings cannot contain white space, and there is no support for strings that would contain white space (this could be introduced later). This Forth is designed to manipulate numeric values into colors to animate RGB LED strings, so string processing is not really a core requirement. (note: the alpha LED message board functions introduce this need so more complete string processing might be added soon).

An int array is a fixed array of integers. These arrays can be used to hold color values (for example hue, saturation, and value; or red, green, blue; or packed RGB values). This would typically be used to compute whole arrays of color values matching a whole LED string. Most of the built-in operators have been enhanced to process arrays by applying themselves to each element of the input arrays in turn and generally returning an array result, so this kind of color processing can be very fast and easy.

Finally, a sequence is a bunch of words representing either the body of a function or a code fragment to be executed conditionally or repeatedly. A sequence can be declared as follows:

```
[ 5 + . ] 0 10 loop cr
```

The sequence is the portion `5 + .` that is enclosed by brackets. Note the white space surrounding each token, including the brackets. The opening and closing bracket are not delimiters, they are words. The left bracket `[` causes the following words to be collected into a sequence data structure. The right bracket `]` causes the collected sequence of words to be closed and then pushed onto the execution stack as a value of type sequence.

This defines a sequence that adds 5 to an input argument, and then prints it. The sequence is executed 10 times by 'loop', with arguments 0..9 in turn. This results in the following output being printed to the serial port:

`prints> 5 6 7 8 9 10 11 12 13 14 \n`

## Stash Stack

Almost all computation happens on the execution stack. A parallel stack called the 'stash' stack can also be used to temporarily hold values. This can sometimes be valuable in avoiding some complex swapping and rotating of the execution stack.

For example, the following word definition takes the product of three separate sums: six input arguments, one value returned.

```
:product-sum-3
	+ >>> + >>> + <<< * <<< * ;
```

| Execution | Execution Stack (after - TOS on right) | Stash Stack |
| ----- | ----- | ----- |
| (input arguments) | 1 2 3 4 5 6 | |
| + | 1 2 3 4 11 | |
| >>> | 1 2 3 4 | 11 |
| + | 1 2 7 | 11 |
| >>> | 1 2 | 11 7 |
| + | 3 | 11 7 |
| <<< | 3 7 | 11 |
| * | 21 | 11 |
| <<< | 21 11 | |
| * | 231 | |

Note that '>>>' moves the top of execution stack to the stash stack, and '<<<' does the reverse. See below for more about the `:` sigil and the `;` word, used to define a sequence and bind it to a global variable.

## Global Dictionary

Variables and defined words are held in a single global dictionary. There is no concept of other scopes, no binding of arguments to a function activation scope, etc., just a single global scope where everything is defined.

This is part of the extreme simplicity of the language design. It does mean that there is a risk of collisions in the global dictionary, so the prefix convention below is used to manage this somewhat.

Words can be defined (`def`) and forgotten (`forget`).

## Prefix Convention

Words can contain almost any non-whitespace character. A colon ':' character is conventionally used to designate a prefix or namespace to keep related words organized, and to disambiguate similar or identical words that operate on different data types. This is different from the leading colon (`:` sigil) that is used to define a global sequence.

For example, the prefix "pin:" is used to organize and distinguish all of the words that manipulate the GPIO pins.

## Unu Comment Format

Ok, I can't find a reference to this commenting convention now, but I think it was called "unu". Forth files that are read from the SD card are assumed to be in this format, which allows alternating comment and code sections. The document opens in 'comment' mode, ignoring lines until a line is encountered that contains three tildes ~~~ in the first three columns. This is a signal to switch to the other mode.

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
	360 array identity 100 5 led:hsv>
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

`:word` starts a sequence definition where the sequence that follows will be defined globally as `word`. The sequence is terminated with the `;` word.

`(anything` defines a comment. The entire word, terminated by whitespace, is ignored. This is typically used to define an input / output comment in a sequence definition.

```
:num:dadd (ii-d)
	+ 0.0 + ;
```

The sequence `[ + 0.0 + ]` is bound to the word `num:dadd`. The comment indicates that two integer inputs are expected, and a single double output is returned.

## Defining Words

As you can see in the above and previous examples, new words can be defined in a couple of ways. Perhaps the most readable is the colon shortcut used above. The colon sigil starts the definition of a new sequence, that sequence consisting of every word that follows until the ';' semicolon word is encountered to terminate the sequence definition.

```
:thing1
	* rot * + sqrt ;
```
is equivalent to:
```
[ * rot * + sqrt ] 'thing1 def
```
but the first form seems more readable, and looks something like a function definition in other languages.


Note that the : sigil places the symbol to be bound at the beginning of the form, which is counter to everything else about the Forth language, as the implementation of that sigil must remember the word being defined and then use that word when the terminating semicolon is found. Thus function definitions are not reentrant, but that seems reasonable given the extremely simple structure of the language and data structures.

# Built-In Words

The following sections describe built-in words and their use.

Where examples are given, they assume an empty stack, so all arguments required by the built-in words are shown. The result is shown to the right of '-->' as a stack with the top-of-stack to the right.

## Math Primitives

### + - * / mod
These are binary operators that expect two arguments and return one.

`1 2 + 3 +` --> `6`

### sq
Square a numeric input.

`5 sq` --> `25`

### sqrt
Return the square root of a numeric input.

`25 sqrt` --> `5.0`

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

Raise the first argument to the power of the second argument.

`2 5 pow` --> `32`

`5 2 pow` --> `25`

### abs

Return the absolute value of an input.

### min max

Return the min or max of a pair of inputs.

### round ceil floor

Return the rounded value, ceiling, or floor, of a single numeric input.

## Stack Manipulations

### dup

Duplicate the top-of-stack.

`1 2 dup` --> `1 2 2`


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

Rotates the top three value in the stack such that the previous top-of-stack is now third.

`1 2 3 rot` --> `3 1 2`

### rup

Rotates the top three values on the stack in reverse of `rot`, such that the previously third item is now top.

`1 2 3 rup` --> `2 3 1`

### rot4

Same as `rot`, but applies to the top four items.

`1 2 3 4 rot4` --> `4 1 2 3`

### rup4

Same as `rup`, but applies to the top four items.

`1 2 3 4 rup4` --> `2 3 4 1`

### rotn

Same as `rot`, but takes an additional argument specifying how many stack elements to rotate.

`1 2 3 4 5 2 rotn` --> `1 2 3 5 4`

`1 2 3 4 5 3 rotn` --> `1 2 5 3 4`

`1 2 3 4 5 5 rotn` --> `5 1 2 3 4`

### rupn

Same as `rup`, but takes an additional argument specifying how many stack elements to rotate.

`1 2 3 4 5 2 rupn ` --> `1 2 3 5 4`

`1 2 3 4 5 3 rupn ` --> `1 2 4 5 3`

`1 2 3 4 5 5 rupn ` --> `2 3 4 5 1`

### drop

Drop the top-of-stack.

`1 2 3 drop` --> `1 2`

### dup2

Duplicate the top two elements of the stack.

`1 2 dup2` --> `1 2 1 2`

### drop2

Drop the top two elements of the stack.

`1 2 3 4 drop2` --> `1 2`

### clst

Clear the entire evaluation stack.

### >>>

Pop the top element of the evaluation stack, and push it onto the stash stack.

### <<<

Pop the top element of the stash stack, and push it onto the evaluation stack.

### < swap >

Swap the entire contents of the evaluation stack and the stash stack.

### clstash

Clear the entire contents of the stash stack.

## Array of Integer operations

Arrays of integer are a basic data type because manipulating color values for an array of RGB LEDs is one of the basic objectives for the language. Arrays of int are created with `array` and then can be manipulated directly using any basic operator such as + - * / and so on.

Most numeric operators will check if their arguments are simple integer values or arrays, and will apply the operation across the entire array.

`5 array identity` --> `(0,1,2,3,4)`

`5 *` --> `(0,5,10,15,20)`

`20 +` --> `(20,25,30,35,40)`

`37 gt` --> `(0,0,0,0,1)`


### sum

Return the sum of an array.

### array

Create and push a new array of int. The size of the array is an input argument, and the returned array is initalized to zeros.

`5 array` --> `(0,0,0,0,0)`

### identity

Populate the array at TOS with the index positions in the array.

`10 array identity` --> `(0,1,2,3,4,5,6,7,8,9)`

### geta puta

Get or Put an element of an int array. The array is on the stack, and following the operation the array remains on the stack. The other arguments are consumed normally.

`5 array identity 2 100 puta` --> `(0,1,100,3,4)`

`5 array identity 5 * 1 geta` --> `(0,5,10,15,20) 5`

### dgeta dputa

Get or Put an element of an int array, where the array is bound to a symbol in the global dictionary.

`5 array !mydata` --> `{mydata:(0,0,0,0,0)}`

`'mydata 0 100 dputa` --> `{mydata:(100,0,0,0,0)}`

`'mydata 0 dgeta` --> `100`

### size

Return the size of an array. Does not consume the array from the stack.

`3 array size` --> `(0,0,0) 3`

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

Conditionals and looping use sequences to hold the conditional or repeated sections of code. The sequences are pushed onto the stack and consumed by the operator.

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

### map

Execute a sequence for each element of an array, returning the result into the array.

`5 array identity` --> `(0,1,2,3,4)`

`[ 5 * 7 + ] map` --> `(7,12,17,22,27)`

Note that this last example is equivalent to `5 array identity 5 * 7 +`, because the `*` and `+` operators will implicitly apply themselves to each element of the array argument and return a new array. The `map` operator is more useful in cases where a function or some conditions are being applied to array elements, although many cases can be handled more efficiently by applying boolean logic to an array to produce another masking array populated with 1's and 0's and using multiplication to apply conditional math.

### call

## RGB Colors

Colors can be specified in three ways:

* as an integer, encoded as 0x00RRGGBB, regardless of the RGB ordering of the target LED string
* as separate integers for red, green, and blue (RGB)
* as separate integers for hue, saturation, and value (HSV

The last is the most intuitive for color animations and blends.

Internally the FastLED library (http://fastled.io/) is being used for color management as well as pushing color data out to supported LED strings. The FastLED library uses C++ templates to optimize code targeted at specific LED types, pin numbers, and processor architectures. For this reason, each supported LED type and pin number must be coded in a switch statement, and there is some overhead to using templates to define all of those specific classes. Switches will be added to allow code size to be trimmed as necessary at the expense of flexibility.

One option for attached LED strings is the OCTOWS2812 board. The Octo board outputs onto two CAT-6 cables with four pairs of (ground,signal) lines each to drive eight LED strings simultaneously. I have also built a similar board with TTL logic level output taken to solder pads on the board.

Strings of LEDs can be connected to individual pins (three wire protocols) or pairs of pins (four wire protocols). There is also the special case of enabling a single Octo2812 board for eight strings at once. Each string attached (or eight strings via the Octo board) consumes one "slot" in the Forth library. There are 32 slots available, which should cover most use cases, but that can be modified easily through a #define. 

The general pattern of use is to:

1. initialize one or more strings of LEDs
2. set pixel color values on the string(s)
3. show the pixels

Initializing a string creates a FastLED object and assigns it to one of the 32 available slots. Pixels are set by sending a color value to a pixel of a string, addressing the pixel by slot number and pixel number. Showing the LEDs will update all of the strings at once.

Pixel values can also be set via fill and gradient operations.

## North America Map Project

An example project is the Drivewyze North America map. This project consists of 2208 LEDs in eight strings of 276 LEDs each. Each string of 276 LEDs is formed of alternating strings of 35 LEDs (left-to-right) and 34 LEDs (right-to-left, on the offset). Four such pairs are connected in series to make a single strand, and eight strands make up the map, altogether approximately 35 LEDs wide and 64 rows tall.

This complicated arrangement of LEDs is wired to the Octo interface, and is then addressable as a flat array of 2208 pixels.

The map has a supporting library (in Forth) that helps with the addressing of pixels by an (x,y) coordinate system, and by (lat,long). For example, here is the Forth function that converts (lat,long) into (x,y) coordinates for the specific canvas North America map used in the project:

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

Note that the first line of code takes the sin of the lat and long input values, and stores them in `map:` prefixed variables for use in the following two polynomials. The coefficients for this polynominal were determined by empirically lighting up a number of the LEDs and then reading the latitude and longitude off the map. The data (lat,long) and (x,y) of the LEDs were entered into a spreadsheet and fit.

## Color Representations

All of the following functions support mixed integer and array operands.

### led:hue>

Convert a hue (int) to a packed int using saturation of 100 and value of 100, producing a bright a fully saturated pixel.

### led:huemed>

Convert a hue (int) to a packed int using saturation of 75 and value of 50, producing a mid-brightness mostly saturated pixel.

### led:huepale>

Convert a hue (int) to a packed int using saturation of 50 and value of 50, producing a mid-brightness pastel color.

### led:rgb>

Convert r, g, b values into a packed int. Range of values is 0..100 for each.

### led:hsv>

Convert h, s, v values into rgb and then a packed int. Hues are corrected so each rainbow member occupies roughly an equal portion of the hue space. Range of values is 0..359 for hue, and 0..100 for saturation and value.

### led:blend

Mixes two packed RGB values using a ratio.

`0x778899 0xA58832 25 led:blend` --> results in new color that is 25% a and 75% b.


### led:init\:ws2812

```
slot pin numleds led:init:ws2812
```

Initialize a string of WS2812 LEDs. `slot` is the slot (0..31) to use for the FastLED object. `pin` is the pin number and must be one of the pins coded for WS2812 use. `numleds` is the number of LEDs in the string.

### led:init\:octo

```
slot numleds led:init:octo
```

Initialize eight strings of WS2812 LEDs, using an OCTOWS2812 board. `slot` is the slot (0..31) to use for the FastLED object. `numleds` is the number of LEDs in one of the strings.

The octo board is commonly used with eight strings of the same length, but it can be used with varying lengths of strings, or less than eight strings. An addressable array of pixels is set aside assuming that all of the strings are of length `numleds`, so there are addressable pixels in the range 0 through (numleds*8)-1.

If the strings are shorter or missing, then pixels will simply roll of the end. Each string is addressable starting at 0 (plus an offset of numleds for each string). For example, if strings of length 100, 50, and 25 are attached, then the LEDs will be addressable at pixels 0..99, 100..149, and 200..224. Colors can be set in other pixels, but they will run off the end of shorter strings harmlessly.

Once the octo board is initalized in a slot, pixels are addressed continuously in that slot for all eight of the actual strings.

### led:size

Return the size of an initialized LED string, given the slot number.

### led:order

Set the RGB order of an initialized string.

`slot order led:order`

The order is specified as an integer:

0: RGB
1: RBG
2: GBR
3: GRB
4: BRG
5: BGR

### led:setmap

Sometimes, the order of pixels in a string (or set of strings in the Octo case) may not be an easy or natural pixel ordering for animations. For example, one might want to create a pixel numbering scheme that puts pixel 0 at the bottom-left of a matrix. Another example could be mis-wiring of strings attached to an Octo board. Another example might be strings that are laid out vertically in an up-and-down alternating arrangement, where the natural pixel ordering for animations should be consistently left-to-right.

In cases such as these, functions can be written to translate the ideal pixel order for animations into the physical mapping. The function `led:setmap` takes this a step further by allowing a translation map to be built once at startup and asserted as the pixel translation for any pixel setting functions.

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
0 swap led:setmap
```

The rows starting at indices 276, 552, 1380, and 1656 were wired correctly and map to themselves. Rows at index 0 and 828 are swapped, and rows at 1104 and 1932 are swapped.

### led:show

Show all of the slots.

### led:c

```
slot index c led:c
```

Set an LED to a color in integer 0x00RRGGBB format. This function and all of those that follow refer to an individual LED by the slot to which the string is assigned, and the index of the LED(s) within the string.

### led:hue

```
slot index hue led:hue
```

Set an LED to a color by hue in the range 0..359, with red at 0. The saturation and value are both set to 100%.

### led:huemed

```
slot index hue led:huemed
```

Set an LED to a color by hue in the range 0..359, with red at 0. The saturation and value are both set to 75% and 50% respectively, resulting in a gentler version of the color.

### led:huepale

```
slot index hue led:huepale
```

Set an LED to a color by hue in the range 0..359, with red at 0. The saturation and value are both set to 50%, resulting in a pastel color.

### led:hsv

```
slot index hue sat val led:hsv
```

Set an LED to a color by hue (0..359), saturation (0..100), and value (0..100).

### led:rgb

```
slot index red green blue led:rgb
```

Set an LED to a color by red (0..100), green (0..100), and blue (0..100).

### led:fill\:c
### led:fill\:hue
### led:fill\:huemed
### led:fill\:huepale
### led:fill\:hsv
### led:fill\:rgb

```
slot index count c led:fill:c
slot index count hue led:fill:hue
slot index count hue led:fill:huemed
slot index count hue led:fill:huepale
slot index count hue sat val led:fill:hsv
slot index count red green blue led:fill:rgb
```

Set a range of LEDs to the same color value. These 'led:fill' functions use a starting index and count to specify the fill range. The color parameters are the same as for the corresponding pixel functions above.

### led:grad\:c
### led:grad\:hue
### led:grad\:huemed
### led:grad\:huepale
### led:grad\:hsv
### led:grad\:rgb

```
slot index count c1 c2 led:grad:c
slot index count hue1 hue2 led:grad:hue
slot index count hue1 hue2 led:grad:huemed
slot index count hue1 hue2 led:grad:huepale
slot index count hue1 sat1 val1 hue2 sat2 val2 led:grad:hsv
slot index count red green blue led:grad:rgb
```

Set a range of LEDs to a gradient from one color value to another. These 'led:grad' functions use a starting index and count to specify the pixel range. The color parameters are the same as for the corresponding pixel functions above.



### led:reada

TODO

```
2208 array slot led:reada
```

Read the current LED array for a given slot into a provided int array. This is useful, for example, to animate a transition from whatever the current LED values are to a new set of values.

### octo:dma-wait

TODO

When the octo library writes its internal array out to the LEDs, it uses memory buffers and DMA (direct memory access) to write the whole pixel array at 800 KHz. This operation takes some time, usually a few milliseconds, but the main processor is not required and control returns immediately to the running process.

When writing a set of frames as an animation, it will produce unknown or unpredictable results if frames are written while the previous frame is still being rendered by the DMA process.

`octo:dma-wait` will wait until the current DMA process is writing data, and will return as soon as the frame is complete. To get a maximum frame rate without overlapping frames, use `octo:dma-wait` just before writing a new frame of data, to make sure the previous frame has completed. This approach will optimize use of the main processor to prepare the next frame, and then render it as soon as possible (but no sooner).


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
