#include <bits/stdc++.h>
#include <openssl/sha.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <thread>

using namespace std;

#define SA struct sockaddr
#define ll long long int

bool is_logged_in;
string tracker1_ip, client_ip;
uint16_t tracker1_port, client_port;
unordered_map<string, string> file_to_file_path;
unordered_map<string, vector<int>> file_chunk_info;
unordered_map<string, unordered_map<string, bool>> upload_check;
vector<vector<string>> check_downloaded_file_chunk;
unordered_map<string, string> list_downloaded_files;
vector<string> file_chunk_hash_check;
bool is_file_corrupt;

class client_file_detail
{
public:
	ll fsize;
	string c_ip;
	string c_file_name;
};

class chunk_details
{
public:
	ll chunk_number;
	string cls_ip;
	string filename;
	string destination_path_gen;
};

void log(string text)
{
	ofstream log_file("log.txt", ios_base::out | ios_base::app);
	log_file << text;
	log_file.close();
}

void synch(int sock)
{
	log("Synchronizing...\n");
	char test[5];
	strcpy(test, "test");
	write(sock, test, 5);
}

vector<string> split_string(string text, char delimiter)
{
	// split string based on given delimiter
	vector<string> result;
	string temp = "";
	for (size_t i = 0; i < text.size(); i++)
	{
		if (text[i] == delimiter)
		{
			result.push_back(temp);
			temp = "";
		}
		else
		{
			temp += text[i];
		}
	}
	result.push_back(temp);

	return result;
}

ll get_file_size(char *path)
{
	ll file_size = -1;
	FILE *f = fopen(path, "rb");
	if (f)
	{
		fseek(f, 0, SEEK_END);
		file_size = ftell(f) + 1;
		fclose(f);
	}
	else
	{
		log("File not found\n");
		cout << "File not found" << endl;
	}
	return file_size;
}

int connect_to_tracker(struct sockaddr_in &serv_addr, int server_socket)
{
	log("Connecting to tracker\n");
	// connect client to tracker
	char *connected_tracker_ip;
	uint16_t connected_tracker_port;
	connected_tracker_ip = &tracker1_ip[0];
	connected_tracker_port = tracker1_port;

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(connected_tracker_port);

	if (inet_pton(AF_INET, connected_tracker_ip, &serv_addr.sin_addr) <= 0)
	{
		return -1;
	}

	int status = connect(server_socket, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	if (status < 0)
	{
		return -1;
	}

	return 0;
}

void to_string_hash(string &hash, string segval)
{
	unsigned char md[20];
	if (SHA1(reinterpret_cast<const unsigned char *>(&segval[0]), segval.length(), md))
	{
		for (int i = 0; i < 20; i++)
		{
			char buffer[3];
			string bufstr;
			sprintf(buffer, "%02x", md[i] & 0xff);
			bufstr = string(buffer);
			hash += bufstr;
		}

		hash += "$";
	}
	else
	{
		cout << "Error in hashing" << endl;
		hash += "$";
	}
}

string get_hash(char *path)
{
	FILE *f;

	ll file_size = get_file_size(path);

	if (file_size == -1)
	{
		return "$";
	}

	string hash = "";

	char line[32769];

	int segments = file_size / 524288 + 1; // 512 KB into BYTES

	f = fopen(path, "r");
	if (!f)
	{
		cout << "File not found" << endl;
		return "";
	}
	for (int i = 0; i <= segments - 1; i++)
	{
		string segval;
		int my_count = 0;
		int my_count_2;
		while (my_count < 524288 && (my_count_2 = fread(line, 1, min(32767, 524288 - my_count), f)))
		{
			line[my_count_2] = '\0';
			segval = segval + line;
			my_count = my_count + strlen(line);
			memset(line, 0, sizeof(line));
		}
		to_string_hash(hash, segval);
	}
	fclose(f);
	hash.pop_back();
	return hash;
}

string get_file_hash(char *path)
{
	string hash;
	unsigned char md[32];

	ifstream input(path);

	ostringstream buffer;
	buffer << input.rdbuf();

	string data = buffer.str();

	if (SHA256(reinterpret_cast<const unsigned char *>(&data[0]), data.length(), md))
	{
		for (int i = 0; i < 32; i++)
		{
			char buffer[3];
			string strbuf;
			sprintf(buffer, "%02x", md[i] & 0xff);
			strbuf = string(buffer);
			hash += strbuf;
		}
		return hash;
	}
	else
	{
		cout << "Error in Hashing" << endl;
		return hash;
	}
}

int check_if_uploaded(int sock, string filename, vector<string> input)
{
	if (upload_check[input[2]].find(filename) != upload_check[input[2]].end())
	{
		cout << "File is already uploaded" << endl;
		short status = send(sock, "error", 5, MSG_NOSIGNAL);
		if (status != -1)
		{
			return 0;
		}
		else
		{
			cout << "Error sending message" << endl;
			return -1;
		}
	}
	return 1;
}

int upload_file(vector<string> command, int server_socket)
{
	if (command.size() != 3)
	{
		return 0;
	}

	char *file_path = &command[1][0];

	string fname = split_string(string(file_path), '/').back();

	string file_details = "";

	int check_already_uploaded = check_if_uploaded(server_socket, fname, command);
	if (check_already_uploaded != 1)
	{
		return check_already_uploaded;
	}

	upload_check[command[2]][fname] = true;

	file_to_file_path[fname] = string(file_path);

	string piece_wise_hash = get_hash(file_path);

	if (piece_wise_hash == "$")
	{
	}
	else
	{
		string filehash = get_file_hash(file_path);
		string file_size = to_string(get_file_size(file_path));

		file_details += string(file_path) + "$";
		file_details += string(client_ip);
		file_details += ":";
		file_details += to_string(client_port) + "$";
		file_details += file_size + "$";
		file_details += filehash + "$";
		file_details += piece_wise_hash;

		int te = send(server_socket, &file_details[0], strlen(&file_details[0]), MSG_NOSIGNAL);
		if (te == -1)
		{
			// error
			return -1;
		}
		else
		{
			char server_reply[10240] = {0};
			read(server_socket, server_reply, 10240);
			cout << server_reply << endl;

			ll r = stoll(file_size) / 524288 + 1;
			vector<int> tmp(r + 1, 1);
			file_chunk_info[fname] = tmp;
		}
	}

	return 0;
}

int write_chunk(int clientsock, ll chunk_number, char *fpath)
{
	int n;
	char buff[524288];
	string mtemp = "", hash = "";

	for (int total = 0; total < 524288; total = total + n)
	{
		n = read(clientsock, buff, 524287);
		if (n <= 0)
		{
			break;
		}
		else
		{
			buff[n] = 0;

			fstream outfile(fpath, std::fstream::in | std::fstream::out | std::fstream::binary);
			outfile.seekp(chunk_number * 524288 + total, ios::beg);
			outfile.write(buff, n);
			outfile.close();

			string writtenlocst = to_string(total + chunk_number * 524288);
			string writtenloced = to_string(total + chunk_number * 524288 + n - 1);

			mtemp += buff;

			bzero(buff, 524288);
		}
	}

	to_string_hash(mtemp, hash);

	string filename = split_string(string(fpath), '/').back();

	file_chunk_info[filename][chunk_number] = 1;

	return 0;
}

string client_as_server_connect(char *cs_ip, char *cs_port_ip, string command)
{
	struct sockaddr_in c_address;

	int csock = socket(AF_INET, SOCK_STREAM, 0);
	if (csock < 0)
	{
		log("Error in creating Socket\n");
		return "error";
	}

	c_address.sin_family = AF_INET;

	unsigned short cport = stoi(string(cs_port_ip));

	c_address.sin_port = htons(cport);

	if (inet_pton(AF_INET, cs_ip, &c_address.sin_addr) < 0)
	{
		log("Client connection error\n");
	}

	vector<string> inptvect = split_string(command, '$');

	if (connect(csock, (struct sockaddr *)&c_address, sizeof(c_address)) < 0)
	{
		log("Client connection error\n");
	}

	if (inptvect[0] == "get_chunk_vector")
	{
		char sreply[10240] = {0};

		short status = send(csock, &command[0], strlen(&command[0]), MSG_NOSIGNAL);
		if (status == -1)
		{
			log("Error: get_chunk_vector\n");
			return "error";
		}

		int rstatus = read(csock, sreply, 10240);
		if (rstatus < 0)
		{
			log("Error: get_chunk_vector\n");
			return "error";
		}
		else
		{
			close(csock);
			return string(sreply);
		}
	}
	else if (inptvect[0] == "get_chunk")
	{
		short status = send(csock, &command[0], strlen(&command[0]), MSG_NOSIGNAL);
		if (status == -1)
		{
			log("Error: get_chunk\n");
			return "error";
		}
		else
		{
			vector<string> cmdtokens = split_string(command, '$');

			string destination_path = cmdtokens[3];
			ll chunk_number = stoll(cmdtokens[2]);
			string cserverport_ipstr = string(cs_port_ip);
			string chunkNumstr = to_string(chunk_number);

			write_chunk(csock, chunk_number, &destination_path[0]);

			return "something";
		}
	}
	else if (inptvect[0] == "get_file_path")
	{
		char sreply[10240] = {0};

		short status = send(csock, &command[0], strlen(&command[0]), MSG_NOSIGNAL);
		if (status == -1)
		{
			log("Error: get_file_path\n");
			return "error";
		}

		if (read(csock, sreply, 10240) < 0)
		{
			log("Error: get_file_path\n");
			return "error";
		}
		else
		{
			string filename = split_string(command, '$').back();
			file_to_file_path[filename] = string(sreply);
		}
	}

	close(csock);
	return "something";
}

void get_chunk_info(client_file_detail *temp)
{
	vector<string> s_p_addr = split_string(string(temp->c_ip), ':');

	string txn = string(temp->c_file_name);

	string command = "get_chunk_vector$" + txn;

	string response = client_as_server_connect(&s_p_addr[0][0], &s_p_addr[1][0], command);

	int check_size = check_downloaded_file_chunk.size();
	for (size_t i = 0; i < check_size; i++)
	{
		if (response[i] == '1')
		{
			check_downloaded_file_chunk[i].push_back(string(temp->c_ip));
		}
	}
	for (auto a : check_downloaded_file_chunk)
	{
		for (auto b : a)
		{
			cout << b << "\t";
		}
		cout << endl;
	}
}

void get_chunks(chunk_details *temp)
{
	string filename = temp->filename;

	vector<string> s_p_ip = split_string(temp->cls_ip, ':');

	ll chunk_number = temp->chunk_number;

	string destination = temp->destination_path_gen;

	string command = "";
	command += "get_chunk$";
	command += filename + "$";
	command += to_string(chunk_number) + "$";
	command += destination;

	client_as_server_connect(&s_p_ip[0][0], &s_p_ip[1][0], command);
}

void piece_wise_algo(vector<string> clients, vector<string> inpt)
{
	long long filesize = stoll(clients.back());

	check_downloaded_file_chunk.clear();

	clients.pop_back();

	long long segments = filesize / 524288 + 1;

	check_downloaded_file_chunk.resize(segments);

	vector<thread> thread_vector_1;
	vector<thread> thread_vector_2;

	int temp_x = clients.size();
	size_t i = 0;

	for (size_t i = 0; i < temp_x; i++)
	{
		client_file_detail *cp = new client_file_detail();

		cp->fsize = segments;
		cp->c_file_name = inpt[2];
		cp->c_ip = clients[i];

		thread_vector_1.push_back(thread(get_chunk_info, cp));
	}

	for (auto i = thread_vector_1.begin(); i != thread_vector_1.end(); i++)
	{
		if (i->joinable())
		{
			i->join();
		}
	}

	int temp_y = check_downloaded_file_chunk.size();

	for (size_t i = 0; i < temp_y; i++)
	{
		int sz = check_downloaded_file_chunk[i].size();
		if (sz == 0)
		{
			cout << "Some parts missing" << endl;
			return;
		}
	}

	thread_vector_1.clear();

	srand((unsigned)time(0));

	ll segs_recd = 0;

	string dp = inpt[3] + "/" + inpt[2];

	FILE *fp = fopen(&dp[0], "r+");
	if (fp == 0)
	{
		string ss(filesize, '\0');
		fstream in(&dp[0], ios::out | ios::binary);

		in.write(ss.c_str(), strlen(ss.c_str()));
		in.close();

		file_chunk_info[inpt[2]].resize(segments, 0);

		vector<int> tmp(segments, 0);

		is_file_corrupt = false;

		file_chunk_info[inpt[2]] = tmp;

		string clientfpath;

		while (segs_recd < segments)
		{
			ll randompiece;
			while (true)
			{
				randompiece = rand() % segments;
				if (file_chunk_info[inpt[2]][randompiece] != 0)
				{
					continue;
				}
				else
				{
					break;
				}
			}

			ll peer_having_piece = check_downloaded_file_chunk[randompiece].size();
			string get_random_peer = check_downloaded_file_chunk[randompiece][rand() % peer_having_piece];

			chunk_details *req = new chunk_details();

			req->cls_ip = get_random_peer;
			req->destination_path_gen = inpt[3] + "/" + inpt[2];
			req->chunk_number = randompiece;
			req->filename = inpt[2];

			file_chunk_info[inpt[2]][randompiece] = 1;

			thread_vector_2.push_back(thread(get_chunks, req));
			segs_recd++;
			clientfpath = get_random_peer;
		}

		for (auto i = thread_vector_2.begin(); i != thread_vector_2.end(); i++)
		{
			if (i->joinable())
			{
				i->join();
			}
		}

		list_downloaded_files.insert({inpt[2], inpt[1]});

		if (is_file_corrupt == false)
		{
			log("Download successful\n");
			cout << "Download completed successfully" << endl;
		}
		else
		{
			log("File corrupted\n");
			cout << "File corrupted" << endl;
		}

		vector<string> serverAddress = split_string(clientfpath, ':');

		string temp_command = "get_file_path$" + inpt[2];
		client_as_server_connect(&serverAddress[0][0], &serverAddress[1][0], temp_command);
	}
	else
	{
		cout << "File already exists" << endl;
		fclose(fp);
	}
}

void send_chunk(char *file_path, int chunk_number, int csock)
{
	std::ifstream fdval(file_path, std::ios::in | std::ios::binary);
	fdval.seekg(chunk_number * 524288, fdval.beg);

	char buff[524288] = {0};

	fdval.read(buff, sizeof(buff));

	int count = fdval.gcount();

	int my_count_2 = send(csock, buff, count, 0);

	if (my_count_2 == -1)
	{
		log("Error in sending file\n");
		exit(1);
	}

	fdval.close();
}

void peer_request_handler(int csock)
{
	string clientid = "";
	char cs_reply[1024] = {0};
	if (read(csock, cs_reply, 1024) <= 0)
	{
		close(csock);
		return;
	}

	vector<string> inputvect = split_string(string(cs_reply), '$');

	if (inputvect[0] == "get_chunk_vector")
	{
		string temp = "";
		vector<int> chunk_vec = file_chunk_info[inputvect[1]];

		for (int i = 0; i < chunk_vec.size(); i++)
		{
			temp += to_string(chunk_vec[i]);
		}

		char *reply = &temp[0];
		write(csock, reply, strlen(reply));

		close(csock);
		return;
	}
	else if (inputvect[0] == "get_file_path")
	{
		string fpath = file_to_file_path[inputvect[1]];
		write(csock, &fpath[0], strlen(fpath.c_str()));

		close(csock);
		return;
	}
	else if (inputvect[0] == "get_chunk")
	{
		ll chunk_number = stoll(inputvect[2]);
		string fpath = file_to_file_path[inputvect[1]];

		log("Sending chunk number: " + to_string(chunk_number) + "\n");
		send_chunk(&fpath[0], chunk_number, csock);

		close(csock);
		return;
	}

	close(csock);
	return;
}

void *client_as_server(void *arg)
{
	int server_socket;
	struct sockaddr_in address;
	int opt = 1;
	int addr_len = sizeof(address);

	if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0)
	{
		log("Socket creation failed!\n");
		exit(EXIT_FAILURE);
	}
	if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
	{
		log("setsockopt error!\n");
		exit(EXIT_FAILURE);
	}
	address.sin_family = AF_INET;
	address.sin_port = htons(client_port);

	if (inet_pton(AF_INET, &client_ip[0], &address.sin_addr) <= 0)
	{
		log("Address not supported!\n");
		exit(EXIT_FAILURE);
	}

	if (bind(server_socket, (SA *)&address, sizeof(address)) < 0)
	{
		log("Bind failed!\n");
		exit(EXIT_FAILURE);
	}

	if (listen(server_socket, 3) < 0)
	{
		log("Socket listen failed!\n");
		exit(EXIT_FAILURE);
	}

	vector<thread> thread_vector;

	while (true) // listen for new clients and create new thread for every new client
	{
		log("Waiting for peers...\n");

		int client_socket;

		if ((client_socket = accept(server_socket, (SA *)&address, (socklen_t *)&addr_len)) < 0)
		{
			log("Error in accepting client! Client socket: " + to_string(client_socket) + "\n");
		}
		else
		{
			log("Connection Accepted for client socket: " + to_string(client_socket) + "\n");
			thread_vector.push_back(thread(peer_request_handler, client_socket));
		}
	}

	// join every thread which has completed its execution
	for (auto i = thread_vector.begin(); i != thread_vector.end(); i++)
	{
		if (i->joinable())
		{
			i->join();
		}
	}

	close(server_socket);
}

int process_command(int server_socket, vector<string> inputvect)
{
	char server_reply_buffer[10240];
	bzero(server_reply_buffer, 1024);
	read(server_socket, server_reply_buffer, 10240);
	cout << server_reply_buffer << endl;
	string server_reply = string(server_reply_buffer);

	if (server_reply == "Invalid argument count")
	{
		log("Invalid argument count");
		return 0;
	}

	if (inputvect[0] == "login")
	{
		if (server_reply == "Username/Password incorrect")
		{
			log("Username/Password incorrect\n");
		}
		else if (server_reply == "You already have one active session")
		{
			log("You already have one active session\n");
		}
		else
		{
			string caddress = client_ip + ":" + to_string(client_port);

			is_logged_in = true;

			write(server_socket, &caddress[0], caddress.length());

			log("Login successful\n");
		}
		return 0;
	}
	else if (inputvect[0] == "logout")
	{
		if (is_logged_in == false)
		{
			log("You are not logged in\n");
		}
		else
		{
			is_logged_in = false;
			log("Logout successful\n");
		}
	}
	else if (inputvect[0] == "create_group")
	{
		log(server_reply + "\n");
		return 0;
	}
	else if (inputvect[0] == "join_group")
	{
		log(server_reply + "\n");
		return 0;
	}
	else if (inputvect[0] == "leave_group")
	{
		log(server_reply + "\n");
		return 0;
	}
	else if (inputvect[0] == "list_requests")
	{
		log(server_reply + "\n");
		return 0;
	}
	else if (inputvect[0] == "accept_request")
	{
		log(server_reply + "\n");
		return 0;
	}
	else if (inputvect[0] == "list_groups")
	{
		log(server_reply + "\n");
		return 0;
	}
	else if(inputvect[0]=="show_downloads")
    {
        for(auto xyz: list_downloaded_files)
        {
            cout << "[C] " << xyz.second << " " << xyz.first << endl;
        }
        return 0;  
    }
	else if (inputvect[0] == "upload_file")
	{
		if (server_reply == "Group not found")
		{
			cout << "Group not found" << endl;
			return 0;
		}
		else if (server_reply == "You are not a member of the group")
		{
			cout << "You are not a member of the group" << endl;
			return 0;
		}
		else if (server_reply == "File not found")
		{
			cout << "File not found" << endl;
			return 0;
		}
		return upload_file(inputvect, server_socket);
	}
	else if (inputvect[0] == "download_file")
	{
		if (server_reply == "Group not found")
		{
			cout << "Group not found" << endl;
			return 0;
		}
		else if (server_reply == "You are not a member of the group")
		{
			cout << "You are not a member of the group" << endl;
			return 0;
		}
		else if (server_reply == "Destination path not found")
		{
			cout << "Destination path not found" << endl;
			return 0;
		}
		string det_file = "";
		if (inputvect.size() == 4)
		{
			det_file += inputvect[2] + "$" + inputvect[3] + "$" + inputvect[1];

			char sreply[524288] = {0};

			int ssv = send(server_socket, &det_file[0], strlen(&det_file[0]), MSG_NOSIGNAL);
			if (ssv == -1)
			{
				printf("Error: %s\n", strerror(errno));
				return -1;
			}

			read(server_socket, sreply, 524288);

			if (string(sreply) != "File not found")
			{
				vector<string> pwf = split_string(sreply, '$');
				cout << "Peers with file:\n";
				for (auto a : pwf)
				{
					cout << a << "\t";
				}
				cout << endl;

				synch(server_socket);

				bzero(sreply, 524288);
				read(server_socket, sreply, 524288);

				vector<string> tmp = split_string(string(sreply), '$');

				file_chunk_hash_check = tmp;

				piece_wise_algo(pwf, inputvect);
			}
			else
			{
				cout << sreply << endl;
			}
		}
		return 0;
	}
}

int main(int argc, char *argv[])
{
	log("\n----------------------------------------------------------------\n");

	ifstream tracker_info_file("tracker_info.txt");
	if (!tracker_info_file)
	{
		log("Error opening tracker_info.txt file\n");
		cout << "Tracker 1 IP: ";
		cin >> tracker1_ip;
		cout << "Tracker 1 PORT: ";
		cin >> tracker1_port;
	}
	else
	{
		getline(tracker_info_file, tracker1_ip);

		string port;
		getline(tracker_info_file, port);
		tracker1_port = stoi(port);
	}
	cout << tracker1_ip << ":" << tracker1_port << endl;

	if (argc > 2)
	{
		client_ip = argv[1];
		client_port = stoi(argv[2]);
	}
	else
	{
		cout << "Client IP: ";
		cin >> client_ip;
		cout << "Client PORT: ";
		cin >> client_port;
	}
	
	int client_socket = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in server_address;

	if (client_socket < 0)
	{
		log("Error in Creating Socket!\n");
		exit(EXIT_FAILURE);
	}

	// thread(client_as_server);
	pthread_t server_thread; // server thread
	int threadval = pthread_create(&server_thread, NULL, client_as_server, NULL);
	if (threadval == -1) // Creating a new thread and checking if it is created successfully or not
	{
		perror("pthread");
		exit(EXIT_FAILURE);
	}

	if (connect_to_tracker(server_address, client_socket) < 0)
	{
		log("Error connecting to tracker!\n");
		exit(EXIT_FAILURE);
	}
	log("Connected to tracker\n");

	while (true)
	{
		string input_line;

		getline(cin, input_line);
		if (input_line.length() < 0)
		{
			continue;
		}

		vector<string> command = split_string(input_line, ' ');

		if (command[0] == "exit")
		{
			log("Exiting...\n");
			break;
		}

		if (is_logged_in == true && command[0] == "login")
		{
			cout << "You have one active session!" << endl;
			continue;
		}

		if (is_logged_in == false && command[0] != "create_user" && command[0] != "login")
		{
			cout << "You need to create account or login first!" << endl;
			continue;
		}

		if (send(client_socket, &input_line[0], strlen(&input_line[0]), MSG_NOSIGNAL) == -1)
		{
			log("Error in sending message to tracker!\n");
			printf("Error: %s\n", strerror(errno));
			return -1;
		}

		log("Processing command\n");
		process_command(client_socket, command);
	}

	close(client_socket);
	return 0;
}