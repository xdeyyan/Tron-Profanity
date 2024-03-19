@echo off
:start
profanity.exe --matching TCPV1F6cKV62UyKStyugB1ggxMY1E3CCCC --output Trx-address.txt --prefix-count 1 --suffix-count 4
goto start
pause


REM //中文
REM //matching TCPV1F6cKV62UyKStyugB1ggxMY1E3CCCC  //代表选中地址
REM //output Trx-address.txt //代表GPU计算出地址后写入到Trx-address.txt这个文本内
REM //prefix-count 2         //代表匹配matching参数地址的前缀2位
REM //uffix-count 4          //代表匹配matching参数地址的尾缀4位



REM //English
REM //matching TCPV1F6cKV62UyKStyugB1ggxMY1E3CCCC //Represents the selected address
REM //output Trx-address.txt //Represents the GPU to calculate the address and write it into the text Trx-address.txt
REM //prefix-count 2 //Represents the prefix 2 digits of the matching parameter address
REM //uffix-count 4 //Represents the suffix 4 bits of the matching parameter address