@echo off
:start
profanity.exe --matching profanity.txt --output Trx-address.txt --prefix-count 2 --suffix-count 4
goto start
pause


REM //中文
REM //matching profanity.txt //代表使用profanity.txt内的地址
REM //output Trx-address.txt //代表GPU计算出地址后写入到Trx-address.txt这个文本内
REM //prefix-count 2         //代表匹配profanity.txt文本内前缀2位
REM //uffix-count 4          //代表匹配profanity.txt文本内尾缀4位

REM //English
REM //matching profanity.txt //Represents the use of the address in profanity.txt
REM //output Trx-address.txt //Represents the GPU to calculate the address and write it into the text Trx-address.txt
REM //prefix-count 2 //Represents matching 2 prefixes in the text of profanity.txt
REM //uffix-count 4 //Represents matching 4 digits of the suffix in the profanity.txt text