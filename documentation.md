

Layers
=======
	there are a maximum of eight layers, 
	the first layer is drawn first and therefore most in the back


Instructions
=============
   _for_the_following_commands_if_an_argument_is_prefixed_by_ _'$'_ _the_ 
   _value_inside_the_specified_register_is_taken_the_value_after_ _'$'_ _must_be_a_number_
	

   Label
-

	* **. .label  .** 		 set a label, can be any string prefixed by "."
	
   Manipulating registers
-
	* **. set a b .**	 set register a to b

	  
	* **. add a b .** 	 add b to register a
	* **. cmp a b .** 	 compare a and b used for following conditional jump instruction
	
	* **. drw f   .** 	 draw current image for f frames 		 
	* **. img i   .**  	 set current image to i  can be an index or a string  look add image list
	
	
   Jump instruction
-
	
	* **. jmp .label .**   jump unconditionally to a line. 
				 the argument can be a line number or a defined label prefixed with '.'
	
	* **. jeq .label .**  jump if last cmp compared two equal values
	* **. jgt .label .**  jump if in last cmp the first value was larger than the second one
	* **. jlt .label .**  jump if in last cmp the first value was less than the second one
	* **. jge .label .**  jump if in last cmp the first value was larger than or equal to the second one
	* **. jle .label .**   jump if in last cmp the first value was less than or equal to the second one

	// Move and shift images	
	* **. mvt x y .** 	 move image to x,y   _//_not_yet_implemented_
	* **. mov x y .** 	 move image by x,y pixels
	* **. shf x y .**	 shift the pixels of the current image by x,y loops pixels 



pxa file format
================

		eight times 
			unsigned 16 bit integer     -- string length of each layer

		eight times
			variable length utf8 string -- code for each layer

		unsigned  8 bit integer      -- number of images
		unsigned 16 bit integer      -- length of buffer for image paths
		variable length utf8 string  -- buffer for image paths
