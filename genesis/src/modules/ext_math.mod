##  object.method_name           function
native $math.minor()              minor
native $math.major()              major
native $math.add()                add
native $math.sub()                sub
native $math.dot()                dot
native $math.distance()           distance
native $math.cross()              cross
native $math.scale()              scale  
native $math.is_lower()           is_lower
native $math.transpose()          transpose

## math objects are ext_ because 'math.[ch]' could conflict
objs ext_math.o

