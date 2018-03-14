# cqu-drcom

DrCOM客户端，支持Windows, Linux (Openwrt)

## 编译

使用CMake编译此工程

## 使用说明

编译生成的可执行文件会尝试读取运行目录下的config.txt作为配置文件，文件的格式如下：

``` text
201xxxxx
password
true/false
```

其中第一行为用户名；第二行为密码；第三行标识是否在虎溪，是为true，否则为false
