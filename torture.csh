#!/bin/csh 
while ( 1 == 1 ) 
	make && sh test.sh  || exit -2
end
