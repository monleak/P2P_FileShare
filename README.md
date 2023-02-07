# P2P_FileShare
![This is an image](image/p2p.png)
<p>Define your own P2P file sharing protocol</p>
<ul>
  <li>A tracking server to maintain information about files to be shared and locations of those files</li>
  <li>Tracking server return the file’s information to a client</li>
  <li>Clients work together to get the requested file</li>
</ul>
<hr>

<h2>How to use my code</h2>

1.  Setup Server:
    1.  Run `cd Server` 
    2.  Run `gcc FSserver.c -o FSserver`
    3.  Run `./FSserver` to run tracking server
2.  Setup Client:
    1.  Run `cd Client` 
    2.  Run `gcc FSClient.c -o FSClient`
    3.  Run `./FSClient <port>` to run client with port. This port is not the port of the tracking server.

<details>
<summary><h2>Help command</h2></summary>
<p>These commands are used on the client side.</p>

```
===========================[COMMAND LIST]=============================
Usage: fs <command>


help                        Hiển thị hướng dẫn

list [-p <page>]            Lấy danh sách các file đang được share
find <filename>             Tìm kiếm file theo tên

downloadLocation <path>     Đặt đường dẫn của thư mục chứa file
                            download
download <ID> [-p <pass>]   Yêu cầu download file
share <path> [-p <pass>]    Share file với đường dẫn
                            Không được đặt pass là "****"

quit                        Ngắt kết nối với server
======================================================================
```
  
</details>
