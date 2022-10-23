# Mini_Torrent_A_P2P_Group_Based_File_Sharing_System

A peer-to-peer group based file sharing system (using torrent architecture) where users can upload and download files from the group they belong to. File is divided into chunks of 512KB and Downloads are parallel from multiple peers. Tracker maintains the user, group and file information to assist peers to communicate between them. Uses SHA1 for checking file integrity.

## Usage
### Tracker

1. Run Tracker:

```
cd tracker
g++ tracker.cpp -o tracker
./tracker
```

`tracker_info.txt` contains the IP, Port details of all the trackers. Ex:
```
127.0.0.1
5000
```

### Client:

1. Run Client:

```
cd client
g++ client.cpp -o client -lssl -lcrypto
./client <IP> <PORT>

example: ./client 127.0.0.1 18000
```

2. Create user account:

```
create_user <user_id> <password>
```

3. Login:

```
login <user_id> <password>
```

4. Create Group:

```
create_group <group_id>
```

5. Join Group:

```
join_group <group_id>
```

6. Leave Group:

```
leave_group <group_id>
```

7. List pending requests:

```
list_requests <group_id>
```

8. Accept Group Joining Request:

```
accept_request <group_id> <user_id>
```

9. List All Group In Network:

```
list_groups
```

10. List All sharable Files In Group:

```
list_files <group_id>
```

11. Upload File:

```
upload_file <file_path> <group_id>
```

12. Download File:

```
download_file <group_id> <file_name> <destination_path>
```

13. Logout:

```
logout
```

14. Show_downloads: 

```
show_downloads
```
