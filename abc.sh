#!/bin/bash

#for dir in b c d e f g h i k l m n o p q r s t u v w
for dir in b c d e f g h i
do
./a.out 0 1048576 /mnt/sd${dir} &
done

