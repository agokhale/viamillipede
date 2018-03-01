#!/bin/csh 
while ( 1 == 1 ) 
	make && sh test.sh  || exit -2
	make && sh test.sh grand  || exit -2
end
