#!/bin/bash

flag=0

if [ "$#" \< "4" ]; then                   #Check for correct arguments
	echo "Error: Too few parameters!"      #Print error message
	exit 1                                 #Exit
elif [ "$#" = "5" ]; then                  #If num of arguments is 5
	flag=1                                 #flag on (size [n] command)
fi


while [ "$1" != "" ]                       #Until read all the arguments                          
	do
    if [ "$1" = "-l" ]; then                #-l flag
    	shift                               #go to next argument
        path=$1                             #save it to path variable 
        cd ${path}                          #change directory to path
        shift                               #go to next argument
    elif [ "$1" = "-c" ]; then              #-c flag
    	shift                               #go to next argument 
    	command=$1                          
    	shift
    	if [ "${flag}" = "1" -a "${command}" = "size" ]; then #if flag on and command = size then
    		size=$1                                           # size variable = [n]
    		shift
    	elif [ "${flag}" = "1" ]; then                                #Invalid command 
    		echo "Error: Only command size has extra argument [n]"    #print error message
    		exit 2                                                    #Exit
    	fi
    else
    	echo "Eror: Use only -l <path> and -c <command>"              #Invalid flag, print error message
    	exit 2                                                        #Exit
    fi
done

if [ "${command}" = "list" ]; then                                  #Run command
	echo "`ls -1`"
elif [ "${command}" = "size" ]; then
	if [ "${flag}" = "0" ]; then
		echo "`du -b -S |sort -n | tail -n +2 `"
	else
		echo "`du -b -S |sort -n | tail -n +2 | tail -${size}`"
	fi
elif [ "${command}" = "purge" ]; then
	rm -rf sdi*
else
	echo "Not valid command ${command} to execute"
fi
