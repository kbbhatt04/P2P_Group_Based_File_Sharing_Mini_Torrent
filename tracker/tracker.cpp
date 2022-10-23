#include <bits/stdc++.h>
#include <openssl/sha.h>
#include <sys/socket.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <thread>

using namespace std;

#define SA struct sockaddr
#define ll long long int

string tracker1_ip, tracker2_ip;
uint16_t tracker1_port, tracker2_port;
unordered_map<string, bool> is_logged_in;
unordered_map<string, string> client_port;

// key = groupname,
// value.first.first = group_members,
// value.first.second = pending_group_join_requests,
// value.second.first = filename_of_shared_files
// value.second.second = set of peers_sharing_that_file
unordered_map<string, pair<pair<vector<string>, vector<string>>, pair<vector<string>, vector<unordered_set<string>>>>> groups;

unordered_map<string, string> piece_wise_hash;
unordered_map<string, string> file_size;

void log(string text)
{
	ofstream log_file("log.txt", ios_base::out | ios_base::app);
	log_file << text;
	log_file.close();
}

void synch(int csock)
{
	log("Synchronizing...\n");
	char abc[5];
	read(csock, abc, 5);
}

int remove_from_vector(vector<string> &v, string element)
{
	vector<string>::iterator index = find(v.begin(), v.end(), element);
	if (index != v.end())
	{
		v.erase(index);
		return 0;
	}
	return -1; // element not found
}

bool path_exists(string path)
{
	struct stat buffer;
	return (stat(path.c_str(), &buffer) == 0);
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

int create_user(string username, string password)
{
	// first check if username already exists
	string line, check_username_exists;

	ifstream read_auth_file("auth.txt");

	if (!read_auth_file)
	{
		log("File auth.txt not found!\n");
		return 1;
	}

	while (getline(read_auth_file, line))
	{
		check_username_exists = split_string(line, ' ')[0];
		if (username == check_username_exists)
		{
			return 2; // username already exists
		}
	}

	read_auth_file.close();

	// if username does not already exists, create user successfully
	ofstream write_auth_file;
	write_auth_file.open("auth.txt", ios::out | ios::app); // open in append mode

	if (!write_auth_file)
	{
		log("Cannot open auth.txt in append mode!\n");
		return 1;
	}

	write_auth_file << username << " " << password << endl;

	write_auth_file.close();

	// user created successfully
	log("User created successfully: " + username + "\n");
	return 0;
}

int login(string username, string password)
{
	string line, check_username, check_password;

	ifstream read_auth_file("auth.txt");

	if (!read_auth_file)
	{
		log("File auth.txt not found!\n");
		return 1;
	}

	while (getline(read_auth_file, line))
	{
		check_username = split_string(line, ' ')[0];
		check_password = split_string(line, ' ')[1];

		if (username == check_username)
		{
			if (password == check_password)
			{
				if (is_logged_in.find(username) == is_logged_in.end())
				{
					is_logged_in.insert({username, true});
				}
				else
				{
					if (is_logged_in[username])
					{
						return 1; // already logged in
					}
					else
					{
						is_logged_in[username] = true;
					}
				}
				return 0; // login successful
			}
			else
			{
				return 2; // wrong password
			}
		}
	}

	read_auth_file.close();

	return 2; // username not found
}

int create_group(string groupname, string admin)
{
	if (groups.find(groupname) == groups.end())
	{
		groups[groupname].first.first.push_back(admin);
	}
	else
	{
		return 1; // groupname already exist
	}

	// group created successfully
	return 0;
}

int request_join_group(string groupname, string username)
{
	if (groups.find(groupname) == groups.end())
	{
		// group not found
		return 1;
	}
	else
	{
		groups[groupname].first.second.push_back(username);
	}

	// request to join group sent successfully
	return 0;
}

int accept_join_group(string groupname, string admin, string username)
{
	if (groups.find(groupname) == groups.end())
	{
		// group not found
		return 1;
	}
	else
	{
		vector<string>::iterator index = find(groups[groupname].first.second.begin(), groups[groupname].first.second.end(), username);
		if (index == groups[groupname].first.second.end())
		{
			return 2; // join request not found
		}
		else
		{
			if (admin == groups[groupname].first.first[0])
			{
				groups[groupname].first.first.push_back(username);

				remove_from_vector(groups[groupname].first.second, username);
			}
			else
			{
				return 3; // you are not an admin
			}
		}
	}
	return 0; // user accepted successfully
}

int leave_group(string groupname, string username)
{
	vector<string>::iterator index = find(groups[groupname].first.first.begin(), groups[groupname].first.first.end(), username);
	if (index == groups[groupname].first.first.end())
	{
		return 1; // user not present in group
	}
	else
	{
		remove_from_vector(groups[groupname].first.first, username);
	}
	return 0; // group left successfully
}

string list_pending_join_group_requests(string groupname)
{
	string ans = "";
	for (size_t i = 0; i < groups[groupname].first.second.size(); i++)
	{
		ans += groups[groupname].first.second[i] + "\n";
	}
	return ans;
}

string list_all_groups()
{
	string ans = "";
	for (auto key : groups)
	{
		ans += key.first + "\n";
	}
	return ans;
}

int logout(string username)
{
	if (is_logged_in[username] == false)
	{
		return 1; // user not logged in
	}

	is_logged_in[username] = false;
	return 0; // logout successful
}

int upload_file(string file_path, string groupname, int client_socket, string username)
{
	if (groups.find(groupname) == groups.end())
	{
		// group not found
		return 1;
	}
	if (find(groups[groupname].first.first.begin(), groups[groupname].first.first.end(), username) == groups[groupname].first.first.end())
	{
		// you are not a member of this group
		return 2;
	}
	if (!path_exists(file_path))
	{
		// file does not exist
		return 3;
	}

	string filename = split_string(file_path, '/').back();
	if (find(groups[groupname].second.first.begin(), groups[groupname].second.first.end(), filename) != groups[groupname].second.first.end())
	{
		for (size_t i = 0; i < groups[groupname].second.first.size(); i++)
		{
			if (groups[groupname].second.first[i] == filename)
			{
				groups[groupname].second.second[i].insert(username);
			}
		}
	}
	else
	{
		groups[groupname].second.first.push_back(filename);
		unordered_set<string> s = {username};
		groups[groupname].second.second.push_back(s);
	}

	char file_details[524288] = {0};
	write(client_socket, "Uploading...", 12);

	if (read(client_socket, file_details, 524288))
	{
		string file_details_str = string(file_details);
		if (file_details_str != "error")
		{
			string hash_of_chunks = "";
			vector<string> file_details_vector = split_string(file_details_str, '$');

			size_t i = 4;
			while (i < file_details_vector.size())
			{
				hash_of_chunks = hash_of_chunks + file_details_vector[i];
				if (i == file_details_vector.size() - 1)
				{
					i += 1;
					continue;
				}
				else
				{
					hash_of_chunks = hash_of_chunks + "$";
				}
				i += 1;
			}
			piece_wise_hash[filename] = hash_of_chunks;

			file_size[filename] = file_details_vector[2];
			write(client_socket, "Uploaded", 8);
		}
	}
	return 0;
}

int download_file(string groupname, string filename, string des_path, string username, int client_socket)
{
	if (groups.find(groupname) == groups.end())
	{
		// group not found
		return 1;
	}
	if (find(groups[groupname].first.first.begin(), groups[groupname].first.first.end(), username) == groups[groupname].first.first.end())
	{
		// you are not a member of this group
		return 2;
	}

	const string &s = des_path;
	struct stat buffer;

	if (stat(s.c_str(), &buffer) != 0)
	{
		// destination directory not found
		return 3;
	}

	char file_details[524288] = {0};

	write(client_socket, "Downloading...", 13);

	if (read(client_socket, file_details, 524288))
	{
		string send_reply = "";

		vector<string> fdet = split_string(string(file_details), '$');

		if (find(groups[groupname].second.first.begin(), groups[groupname].second.first.end(), fdet[0]) == groups[groupname].second.first.end())
		{
			write(client_socket, "File not found", 14);
		}
		else
		{
			int temp;

			for (size_t i = 0; i < groups[groupname].second.first.size(); i++)
			{
				if (fdet[0] == groups[groupname].second.first[i])
				{
					temp = i;
					for (auto j : groups[groupname].second.second[i])
					{
						send_reply += client_port[j] + '$';
					}
				}
			}

			send_reply += file_size[fdet[0]];

			write(client_socket, &send_reply[0], send_reply.length());

			synch(client_socket);

			write(client_socket, &piece_wise_hash[fdet[0]][0], piece_wise_hash[fdet[0]].length());

			groups[groupname].second.second[temp].insert(username);
		}
	}
	return 0;
}

void handle_connection(int client_socket)
{
	log("[Thread started for client: " + to_string(client_socket) + "]\n");

	string username = "";
	while (true)
	{
		char read_buffer[1024] = {0};

		if (read(client_socket, read_buffer, 1024) <= 0)
		{
			is_logged_in[username] = false;
			close(client_socket);
			break;
		}

		log("Client request: " + string(read_buffer) + "\n");

		vector<string> command = split_string(string(read_buffer), ' ');

		if (command[0] == "create_user")
		{
			if (command.size() != 3)
			{
				write(client_socket, "Invalid argument count", 22);
			}
			else
			{
				short status = create_user(command[1], command[2]);
				if (status == 2)
				{
					write(client_socket, "Username already exists!", 24);
				}
				else if (status == 1)
				{
					write(client_socket, "File error: auth.txt", 20);
				}
				else
				{
					write(client_socket, "Account created", 15);
				}
			}
		}
		else if (command[0] == "login")
		{
			if (command.size() != 3)
			{
				write(client_socket, "Invalid argument count", 22);
			}
			else
			{
				short status = login(command[1], command[2]);
				if (status == 2)
				{
					write(client_socket, "Username/Password incorrect", 27);
				}
				else if (status == 1)
				{
					write(client_socket, "You already have one active session", 35);
				}
				else
				{
					log("Login Successful: " + to_string(client_socket) + "\n");
					write(client_socket, "Login Successful", 16);

					username = command[1];

					char buf[96];
					read(client_socket, buf, 96);
					string peer_address = string(buf);
					log("Peer Address: " + peer_address + "\n");
					client_port[username] = peer_address;
				}
			}
		}
		else if (command[0] == "create_group")
		{
			log("Create group requested\n");
			if (command.size() != 2)
			{
				write(client_socket, "Invalid argument count", 22);
			}
			else
			{
				short status = create_group(command[1], username);
				if (status == 1)
				{
					write(client_socket, "Group already exists", 20);
				}
				else
				{
					write(client_socket, "Group created", 13);
				}
			}
		}
		else if (command[0] == "join_group")
		{
			if (command.size() != 2)
			{
				write(client_socket, "Invalid argument count", 22);
			}
			else
			{
				short status = request_join_group(command[1], username);

				if (status == 1)
				{
					write(client_socket, "Group does not exist", 20);
				}
				else
				{
					write(client_socket, "Request to join group sent", 26);
				}
			}
		}
		else if (command[0] == "leave_group")
		{
			if (command.size() != 2)
			{
				write(client_socket, "Invalid argument count", 22);
			}
			else
			{
				short status = leave_group(command[1], username);
				if (status == 1)
				{
					write(client_socket, "You are not present in the group", 32);
				}
				else
				{
					write(client_socket, "Group left", 10);
				}
			}
		}
		else if (command[0] == "list_requests")
		{
			log("List requests requested\n");
			if (command.size() != 2)
			{
				write(client_socket, "Invalid argument count", 22);
			}
			else
			{
				string pending_join_requests = list_pending_join_group_requests(command[1]);
				write(client_socket, pending_join_requests.c_str(), pending_join_requests.size() - 1);
			}
		}
		else if (command[0] == "accept_request")
		{
			if (command.size() != 3)
			{
				write(client_socket, "Invalid argument count", 22);
			}
			else
			{
				short status = accept_join_group(command[1], username, command[2]);
				if (status == 1)
				{
					write(client_socket, "Group not found", 15);
				}
				else if (status == 2)
				{
					write(client_socket, "Join request not found", 22);
				}
				else if (status == 3)
				{
					write(client_socket, "You are not an admin", 20);
				}
				else
				{
					write(client_socket, "User added in group", 19);
				}
			}
		}
		else if (command[0] == "list_groups")
		{
			log("List groups requested\n");
			if (command.size() != 1)
			{
				write(client_socket, "Invalid argument count", 22);
			}
			else
			{
				string all_groups = list_all_groups();
				write(client_socket, all_groups.c_str(), all_groups.size() - 1);
			}
		}
		else if (command[0] == "logout")
		{
			if (command.size() != 1)
			{
				write(client_socket, "Invalid argument count", 22);
			}
			else
			{
				short status = logout(username);
				if (status == 1)
				{
					write(client_socket, "User not logged in", 18);
				}
				else
				{
					write(client_socket, "Logged out", 10);
				}
			}
		}
		else if (command[0] == "upload_file")
		{
			log("Upload file requested\n");
			if (command.size() != 3)
			{
				write(client_socket, "Invalid argument count", 22);
			}
			else
			{
				short status = upload_file(command[1], command[2], client_socket, username);
				if (status == 1)
				{
					write(client_socket, "Group not found", 15);
				}
				else if (status == 2)
				{
					write(client_socket, "You are not a member of the group", 33);
				}
				else if (status == 3)
				{
					write(client_socket, "File not found", 14);
				}
			}
		}
		else if (command[0] == "list_files")
		{
			if (command.size() != 2)
			{
				write(client_socket, "Invalid argument count", 22);
			}
			else
			{
				log("List file requested\n");
				string res = "";
				for (size_t i = 0; i < groups[command[1]].second.first.size(); i++)
				{
					res += groups[command[1]].second.first[i] + "\n";
				}
				write(client_socket, res.c_str(), res.size() - 1);
			}
		}
		else if (command[0] == "download_file")
		{
			log("Download file requested\n");
			if (command.size() != 4)
			{
				write(client_socket, "Invalid argument count", 22);
			}
			else
			{
				short status = download_file(command[1], command[2], command[3], username, client_socket);
				if (status == 1)
				{
					write(client_socket, "Group not found", 15);
				}
				else if (status == 2)
				{
					write(client_socket, "You are not a member of the group", 33);
				}
				else if (status == 3)
				{
					write(client_socket, "Destination path not found", 26);
				}
			}
		}
		else if (command[0] == "show_downloads")
        {
            write(client_socket, "Loading...", 10);
        }
	}

	log("[Thread ended for client socket: " + to_string(client_socket) + "]\n");
	close(client_socket);
}

int main(int argc, const char *argv[])
{
	log("\n***********************************************************************\n");

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
	// tracker1_ip = argv[1];
	// tracker1_port = stoi(argv[2]);
	cout << tracker1_ip << ":" << tracker1_port << endl;

	int tracker_socket;
	struct sockaddr_in address;
	int opt = 1;
	int addr_len = sizeof(address);

	if ((tracker_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0)
	{
		log("Socket creation failed!\n");
		exit(EXIT_FAILURE);
	}
	if (setsockopt(tracker_socket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
	{
		log("setsockopt error!\n");
		exit(EXIT_FAILURE);
	}
	address.sin_family = AF_INET;
	address.sin_port = htons(tracker1_port);

	if (inet_pton(AF_INET, &tracker1_ip[0], &address.sin_addr) <= 0)
	{
		log("Address not supported!\n");
		exit(EXIT_FAILURE);
	}

	if (bind(tracker_socket, (SA *)&address, sizeof(address)) < 0)
	{
		log("Bind failed!\n");
		exit(EXIT_FAILURE);
	}

	if (listen(tracker_socket, 3) < 0)
	{
		log("Socket listen failed!\n");
		exit(EXIT_FAILURE);
	}

	vector<thread> thread_vector;

	while (true) // listen for new clients and create new thread for every new client
	{
		cout << "Waiting for client...\n";
		log("Waiting for client...\n");

		int client_socket;

		if ((client_socket = accept(tracker_socket, (SA *)&address, (socklen_t *)&addr_len)) < 0)
		{
			log("Error in accepting client! Client socket: " + to_string(client_socket) + "\n");
		}
		else
		{
			log("Connection Accepted for client socket: " + to_string(client_socket) + "\n");
			thread_vector.push_back(thread(handle_connection, client_socket));
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

	return 0;
}