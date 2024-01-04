#include "peerheader.h"

int idx = 0;
int down = 0;

struct addr_info
{
    int PORT;
    string IP;
};

struct file_info
{
    string file_name;
    int file_size;
    int file_chunks;
    vector<addr_info> peers;
};

unordered_map<string, string> users; // username, password

unordered_map<string, vector<string>> group_info;                                         // [group_name] -> vec[user1, user2, ...]
unordered_map<string, bool> loggedin_users;                                               // [username] -> true/false
unordered_map<string, set<string>> join_requests;                                         // [group_name] -> set[user1, user2, ...]
unordered_map<string, addr_info> user_info;                                               // username, ip,port,socket
unordered_map<string, unordered_map<string, pair<vector<string>, string>>> group_seeders; // [G1][file1]={{c1,c2,c3},hash_full_file}
unordered_map<string, unordered_map<string, vector<string>>> group_leechers;              // [G1][file1]={c1,c2,c3}

#define buff_size 1024
#define chunk_size 524288

string get_path(string f_name)
{
    char cwd[1024];
    getcwd(cwd, sizeof(cwd));
    string path = cwd;
    path += "/test/" + f_name;
    return path;
}

vector<string> tokenize(string str)
{
    string delm = " ";
    vector<string> tokens;
    string token;

    for (int i = 0; i < str.length(); i++)
    {
        if (str[i] == delm[0])
        {
            tokens.push_back(token);

            token = "";
        }
        else
        {
            token += str[i];
        }
    }
    tokens.push_back(token);

    return tokens;
}

bool chkjoin_request(string user, string group)
{
    if (join_requests.find(group) == join_requests.end())
    {
        return false;
    }
    else
    {
        set<string> users = join_requests[group];
        if (users.find(user) == users.end())
        {
            return false;
        }
        else
        {
            return true;
        }
    }
}

bool str_cmp(string str1, string str2)
{
    return (strcmp(str1.c_str(), str2.c_str()) == 0);
}

bool authenticate(string user)
{
    cout << "in authenticate" << endl;
    cout << user << " " << loggedin_users[user] << endl;

    return (loggedin_users[user]);
}

bool group_exists(string group)
{
    if (group_info.find(group) == group_info.end())
    {
        return false;
    }
    else
    {
        return true;
    }
}

bool chk_if_admin(string user, string group)
{

    if (group_info.find(group) == group_info.end())
    {
        return false;
    }
    else
    {
        if (str_cmp(group_info[group][0], user))
            return true;
        else
            return false;
    }
}

bool user_of_group(string user, string group)
{
    if (group_exists(group))
    {
        vector<string> users = group_info[group];
        for (int i = 0; i < users.size(); i++)
        {
            if (str_cmp(users[i], user))
            {
                return true;
            }
        }
    }
    return false;
}

bool sendack(string ack, int client_sd)
{
    cout << "Sending acknolegement:: " << ack << endl;
    int status = send(client_sd, ack.data(), buff_size, 0);
    if (status <= 0)
    {
        cout << "Could not send acknolegement" << endl;
        return false;
    }
    else
    {
        cout << "Acknowledgement Sent Successfully." << endl;
        return true;
    }
}
addr_info sd_to_addr(int client_sd)
{
    addr_info addr;
    struct sockaddr_in addr_in;
    socklen_t addr_size = sizeof(struct sockaddr_in);
    getpeername(client_sd, (struct sockaddr *)&addr_in, &addr_size);
    addr.IP = inet_ntoa(addr_in.sin_addr);
    addr.PORT = ntohs(addr_in.sin_port);
    // addr.socket = client_sd;
    return addr;
}

void save_info(int port, string user)
{
    addr_info addr;
    addr.IP = "127.0.0.1";
    addr.PORT = port;
    user_info[user] = addr;
    // addr.socket = -1;
}

void destroy_info(string user)
{
    user_info.erase(user);
}

void print_info(string user)
{
    cout << "User: " << user << endl;
    cout << "IP: " << user_info[user].IP << endl;
    cout << "PORT: " << user_info[user].PORT << endl;
    // cout << "Socket: " << user_info[user].socket << endl;
}

string populate_files_map(string f_name, string group_id, string hash, string userid)
{
    // chk if user member of group
    if (!user_of_group(userid, group_id))
    {
        return "You are not a member of this group";
    }

    pair<vector<string>, string> seeder_list = group_seeders[group_id][f_name];
    if (seeder_list.first.size() > 0)
    {
        for (int i = 0; i < seeder_list.first.size(); i++)
        {
            if (seeder_list.first[i] == userid)
            {
                return "File already exists";
            }
        }
    }

    group_seeders[group_id][f_name].first.push_back(userid);
    group_seeders[group_id][f_name].second = hash;
    return "File Uploaded Successfully";
}

string handle_download(string group_id, string f_name, string userid, int client_sd)
{
    if (group_exists(group_id))
    {
        if (user_of_group(userid, group_id))
        {
            if (group_seeders.find(group_id) != group_seeders.end())
            {
                if (group_seeders[group_id].find(f_name) != group_seeders[group_id].end())
                {
                    send(client_sd, "File Found", buff_size, 0);

                    pair<vector<string>, string> seeder_list = group_seeders[group_id][f_name];
                    string hash = seeder_list.second;
                    string seeder_ip_port = "";
                    for (auto it : seeder_list.first)
                    {
                        if (loggedin_users[it] == false) //
                        {
                            continue;
                        }

                        addr_info addr = user_info[it];
                        seeder_ip_port += addr.IP + ":" + to_string(addr.PORT);
                        seeder_ip_port += " ";
                        // if (it != seeder_list.first.back())
                        // {
                        //     seeder_ip_port += " ";
                        // }
                    }
                    string msg = seeder_ip_port + hash;
                    // cout << msg << endl;
                    // string seeder = seeder_list[0].first;
                    // string hash = seeder_list[0].second;
                    // string ip = user_info[seeder].IP;
                    // string port = to_string(user_info[seeder].PORT);
                    // string msg = "download " + f_name + " " + hash + " " + ip + " " + port;
                    return msg;
                }
                else
                {
                    return "File does not exist";
                }
            }
            else
            {
                return "File does not exist";
            }
        }
        else
        {
            return "You are not a member of this group";
        }
    }
    else
    {
        return "Group does not exist";
    }
    return "Error";
}

string handle_stop_share(string group_id, string f_name, string userid)
{
    if (group_seeders.find(group_id) != group_seeders.end())
    {
        if (group_seeders[group_id].find(f_name) != group_seeders[group_id].end())
        {
            pair<vector<string>, string> seeder_list = group_seeders[group_id][f_name];
            for (int i = 0; i < seeder_list.first.size(); i++)
            {
                if (str_cmp(seeder_list.first[i], userid))
                {
                    if (seeder_list.first.size() == 1)
                    {
                        group_seeders[group_id].erase(f_name);
                    }
                    else
                    {
                        seeder_list.first.erase(seeder_list.first.begin() + i);
                        group_seeders[group_id][f_name] = seeder_list;
                    }

                    return "File removed successfully";
                }
            }
        }
    }
    else if (group_leechers.find(group_id) != group_leechers.end())
    {
        if (group_leechers[group_id].find(f_name) != group_leechers[group_id].end())
        {
            vector<string> leecher_list = group_leechers[group_id][f_name];
            for (int i = 0; i < leecher_list.size(); i++)
            {
                if (str_cmp(leecher_list[i], userid))
                {
                    leecher_list.erase(leecher_list.begin() + i);
                    group_leechers[group_id][f_name] = leecher_list;
                    return "File removed successfully";
                }
            }
        }
    }

    return "File does not exist";
}

string handle_update_leeches(string group_id, string f_name, string userid)
{
    // add user to leecher list
    group_leechers[group_id][f_name].push_back(userid);
    return "true";
}

string handle_update_seeders(string group_id, string f_name, string userid)
{

    // remove from leecher list if exists

    if (group_leechers.find(group_id) != group_leechers.end())
    {
        if (group_leechers[group_id].find(f_name) != group_leechers[group_id].end())
        {
            vector<string> leecher_list = group_leechers[group_id][f_name];
            for (int i = 0; i < leecher_list.size(); i++)
            {
                if (str_cmp(leecher_list[i], userid))
                {
                    leecher_list.erase(leecher_list.begin() + i);
                    group_leechers[group_id][f_name] = leecher_list;
                    break;
                }
            }
        }
    }

    // add user to seeder list if not already present
    if (group_seeders.find(group_id) != group_seeders.end())
    {
        if (group_seeders[group_id].find(f_name) != group_seeders[group_id].end())
        {
            pair<vector<string>, string> seeder_list = group_seeders[group_id][f_name];
            for (int i = 0; i < seeder_list.first.size(); i++)
            {
                if (str_cmp(seeder_list.first[i], userid))
                {
                    return "true";
                }
            }
            seeder_list.first.push_back(userid);
            group_seeders[group_id][f_name] = seeder_list;
            return "true";
        }
    }
    // group_seeders[group_id][f_name].first.push_back(userid);
    return "true";
}

string handle_group_leave(string group_id, string user_id)
{
    if (group_exists(group_id))
    {
        if (group_info[group_id].size())
        {
            auto it = group_info.find(group_id);
            group_info.erase(it);
            // removing groupid from seeders and leechers list
            if (group_seeders.find(group_id) != group_seeders.end())
            {
                group_seeders.erase(group_id);
            }
            if (group_leechers.find(group_id) != group_leechers.end())
            {
                group_leechers.erase(group_id);
            }
            return "Group deleted as you were the last member";
        }
    }
    return "true";
}

string handle_send_leeches(string group_id, string f_name, string session_user_id)
{
    // sending list of all leeches for a file
    if (group_leechers.find(group_id) != group_leechers.end())
    {
        if (group_leechers[group_id].find(f_name) != group_leechers[group_id].end())
        {
            vector<string> leecher_list = group_leechers[group_id][f_name];
            string leechers = "";
            for (int i = 0; i < leecher_list.size(); i++)
            {
                if (loggedin_users[leecher_list[i]] == false || str_cmp(leecher_list[i], session_user_id))
                {
                    continue;
                }

                addr_info addr = user_info[leecher_list[i]];
                string ip = addr.IP;
                int port = addr.PORT;
                leechers += ip + ":" + to_string(port);
                leechers += " ";

                // if (i != leecher_list.size() - 1)
                // {
                //     leechers += " ";
                // }
            }
            return leechers;
        }
    }
    return "no_leeches";
}

string cmmnd_parser(string cmnd, int client_sd, string &session_userid, string &session_socket)
{

    vector<string> tkns = tokenize(cmnd);
    // : create_user <user_id> <password>
    string ack;
    if (strcmp(tkns[0].data(), "create_user") == 0)
    {
        if (tkns.size() != 3)
        {
            ack = "Expected 3 arguments";
            return ack;
        }

        if (loggedin_users.find(tkns[1]) != loggedin_users.end())
        {
            if (loggedin_users[tkns[1]] == true)
            {
                ack = "User already exists";
                return ack;
            }
            else
            {
                ack = "User already exists";
                return ack;
            }
        }
        if (users.find(tkns[1]) == users.end())
        {
            users[tkns[1]] = tkns[2];
            // cout << "User created" << endl;
            ack = "User created Successfully";
        }
        else
        {
            // cout << "User already exists" << endl;
            ack = "User already exists";
        }
    }
    // login <user_id> <password>
    else if (strcmp(tkns[0].data(), "login") == 0)
    {
        int port;
        if (tkns.size() == 4)
        {

            port = stoi(tkns[3]);
            tkns.pop_back();
        }

        if (tkns.size() != 3)
        {
            ack = "Expected 2 arguments";
            return ack;
        }

        if (users.find(tkns[1]) != users.end())
        {
            if (loggedin_users[tkns[1]])
            {
                ack = "User already logged in";
                return ack;
            }

            if (users[tkns[1]] == tkns[2])
            {
                session_userid = tkns[1];
                loggedin_users[tkns[1]] = true;
                save_info(port, tkns[1]);
                print_info(tkns[1]);
                // cout << "User logged in" << endl;
                ack = "User logged in Successfully";
            }
            else
            {
                // cout << "Incorrect password" << endl;
                ack = "Incorrect password";
            }
        }
        else
        {
            // cout << "User does not exist" << endl;
            ack = "User does not exist";
        }
    }
    // create_group <group_id>
    else if (str_cmp(tkns[0], "create_group"))
    {
        if (tkns.size() != 2)
        {
            ack = "Expected 1 argument:create_group <group_id>";
            return ack;
        }

        if (authenticate(session_userid))
        {
            if (group_info.find(tkns[1]) == group_info.end())
            {

                group_info[tkns[1]].push_back(session_userid);
                // cout << "Group created" << endl;
                ack = "Group created";
            }
            else
            {
                // cout << "Group already exists" << endl;
                ack = "Group already exists";
            }
        }
        else
        {
            // cout << "User not logged in" << endl;
            ack = "User not logged in";
        }
    }
    // join_group <group_id>
    else if (str_cmp(tkns[0], "join_group"))
    {

        if (tkns.size() != 2)
        {
            ack = "Expected 1 argument:join_group <group_id>";
            return ack;
        }

        if (authenticate(session_userid))
        {
            if (group_info.find(tkns[1]) != group_info.end())
            {

                vector<string> users = group_info[tkns[1]];
                for (auto it : users)
                {
                    if (str_cmp(it, session_userid))
                    {
                        // cout << "User already in group" << endl;
                        ack = "User already in group";
                        return ack;
                    }
                }

                join_requests[tkns[1]].insert(session_userid);
                // cout << "Join Request Made" << endl;
                ack = "Join Request Made";
            }
            else
            {
                // cout << "Group does not exist" << endl;
                ack = "Group does not exist";
            }
        }

        else
        {
            // cout << "User not logged in" << endl;
            ack = "User not logged in";
        }
    }
    // leave_group <group_id>
    else if (str_cmp(tkns[0], "leave_group"))
    {
        if (tkns.size() != 2)
        {
            ack = "Expected 1 argument:leave_group <group_id>";
            return ack;
        }

        if (authenticate(session_userid))
        {

            if (group_info.find(tkns[1]) != group_info.end())
            {
                vector<string> users = group_info[tkns[1]];
                bool flag = 0;
                for (int i = 0; i < users.size(); i++)
                {
                    if (str_cmp(users[i], session_userid))
                    {
                        flag = 1;
                        users.erase(users.begin() + i);
                        ack = handle_group_leave(tkns[1], session_userid);
                        if (str_cmp(ack, "true"))
                        {
                            ack = "Successfully left group";
                        }
                        // cout << "User left group" << endl;
                        ack = "Successfully left group";
                        break;
                    }
                }
                group_info[tkns[1]] = users;
                if (!flag)
                {
                    // cout << "User not in group" << endl;
                    ack = "User not in group";
                }
            }
            else
            {
                // cout << "Group does not exist" << endl;
                ack = "Group does not exist";
            }
        }
        else
        {
            // cout << "User not logged in" << endl;
            ack = "User not logged in";
        }
    }

    // list_requests<group_id>
    else if (str_cmp(tkns[0], "list_requests"))
    {
        if (tkns.size() != 2)
        {
            ack = "Expected 1 argument:list_requests<group_id>";
            return ack;
        }

        if (authenticate(session_userid))
        { // admin chk
            if (chk_if_admin(session_userid, tkns[1]) == false)
            {
                ack = "You are not the admin of group";
                return ack;
            }

            if (join_requests.find(tkns[1]) != join_requests.end())
            {
                string res = "";
                for (auto it : join_requests[tkns[1]])
                {
                    res += it + "\n";
                }
                ack = res;
            }
            else
            {
                // cout << "Group does not exist" << endl;
                ack = "Group does not exist";
            }
        }
        else
        {
            // cout << "User not logged in" << endl;
            ack = "User not logged in";
        }
    }
    // accept_request <group_id> <user_id>

    else if (str_cmp(tkns[0], "accept_request"))
    {

        if (tkns.size() != 3)
        {
            ack = "Expected 2 arguments:accept_request <group_id> <user_id>";
            return ack;
        }

        if (authenticate(session_userid))
        {
            // admin_chk:-

            if (group_exists(tkns[1]) && !(user_of_group(tkns[2], tkns[1])))
            {

                if (chk_if_admin(session_userid, tkns[1]) == false)
                {
                    ack = "You are not the admin of group";
                    return ack;
                }

                if (chkjoin_request(tkns[2], tkns[1]))
                {

                    group_info[tkns[1]].push_back(tkns[2]);
                    join_requests[tkns[1]].erase(tkns[2]);
                    // cout << "Request accepted" << endl;
                    ack = "Request accepted";
                }

                else
                {
                    // cout << "User have not made any join request to the group" << endl;
                    ack = "User have not made any join request to the group";
                }
            }
            else
            {
                // cout << "Group does not exist or User not a Member of Group" << endl;
                ack = "Group does not exist or User not a member of Group";
            }
        }
        else
        {
            // cout << "You need to log in first to view any data" << endl;
            ack = "You need to log in first to view any data";
        }
    }
    // list_groups
    else if (str_cmp(tkns[0], "list_groups"))
    {

        if (tkns.size() != 1)
        {
            // ack = "Expected 0 arguments: list_groups";
            return ack;
        }

        if (authenticate(session_userid))
        { // Group count chk:-
            string res = "\nGroup Count: " + to_string(group_info.size()) + "\n";
            for (auto it : group_info)
            {
                res = res + it.first + "\n";
            }
            ack = res;
        }

        else
        {
            // cout << "You need to log in first to view any data" << endl;
            ack = "You need to log in first to view any data";
        }
    }
    // list_files <group_id>
    else if (str_cmp(tkns[0], "list_files"))
    {

        if (tkns.size() != 2)
        {
            ack = "Expected 1 argument:list_files <group_id>";
            return ack;
        }

        if (authenticate(session_userid))
        {
            if (group_exists(tkns[1]))
            {
                if (user_of_group(session_userid, tkns[1]))
                {
                    string res = "\nFile Count: " + to_string(group_seeders[tkns[1]].size()) + "\n";
                    if (group_seeders[tkns[1]].size() == 0)
                    {
                        res = "No files in group";
                    }
                    for (auto it : group_seeders[tkns[1]])
                    {
                        res = res + it.first + "\n";
                    }
                    ack = res;
                }
                else
                {
                    // cout << "User not in group" << endl;
                    ack = "User not in group";
                }
            }
            else
            {
                // cout << "Group does not exist" << endl;
                ack = "Group does not exist";
            }
            // group exist chk, chk group files map.
        }
        else
        {
            // cout << "User not logged in" << endl;
            ack = "User not logged in";
        }
    }
    // upload_file <file_path> <group_id>   --> pre_proccessed f_path to f_name
    else if (str_cmp(tkns[0], "upload_file"))
    {
        string hash;
        if (tkns.size() == 4)
        {
            hash = tkns[3];
            tkns.pop_back();
        }

        if (tkns.size() != 3)
        {
            ack = "Expected 2 arguments: upload_file <file_path> <group_id>";
            return ack;
        }

        if (authenticate(session_userid))
        {
            if (group_info.find(tkns[2]) != group_info.end())
            { // users alredy in group chk, chk if file already exists in group
                ack = populate_files_map(tkns[1], tkns[2], hash, session_userid);
                // ack = "Successfully Uploaded the file";
                return ack;
            }
            else
            {
                // cout << "Group does not exist" << endl;
                ack = "Group does not exist";
            }
        }
        else
        {
            // cout << "User not logged in" << endl;
            ack = "User not logged in";
        }
    }
    // download_file <group_id> <file_name><destination_path>
    else if (str_cmp(tkns[0], "download_file"))
    {

        if (tkns.size() != 3)
        {
            ack = "Expected 2 arguments:download_file <group_id> <file_name><destination_path>";
            return ack;
        }

        if (authenticate(session_userid))
        {
            ack = handle_download(tkns[1], tkns[2], session_userid, client_sd);

            return ack;
        }
        else
        {
            // cout << "User not logged in" << endl;
            ack = "User not logged in";
        }
    }

    else if (str_cmp(tkns[0], "update_leeches"))
    {
        ack = handle_update_leeches(tkns[1], tkns[2], session_userid);
        return ack;
    }

    else if (str_cmp(tkns[0], "update_seeder"))
    {
        ack = handle_update_seeders(tkns[1], tkns[2], session_userid);
        return ack;
    }
    // send_leeches g1 f_name
    else if (str_cmp(tkns[0], "send_leeches"))
    {
        ack = handle_send_leeches(tkns[1], tkns[2], session_userid);
        return ack;
    }

    // logout
    else if (str_cmp(tkns[0], "logout"))
    {

        if (tkns.size() != 1)
        {
            ack = "Expected 0 arguments:logout";
            return ack;
        }

        if (authenticate(session_userid))
        {
            destroy_info(session_userid);
            loggedin_users[session_userid] = 0;
            session_userid = "";
            session_socket = "";
            // cout << "User logged out Successfully!" << endl;
            ack = "User logged out Successfully!";

            return ack;
        }
        else
        {
            // cout << "User not logged in" << endl;
            ack = "User not logged in";
        }
    }

    else if (cmnd == "close_connection")
    {
        ack = "Connection Closed";
        return ack;
    }
    // stop_share <group_id> <file_name>
    else if (str_cmp(tkns[0], "stop_share"))
    {
        if (tkns.size() != 3)
        {
            ack = "Expected 2 arguments:stop_share <group_id> <file_name>";
            return ack;
        }

        if (authenticate(session_userid))
        {
            if (group_exists(tkns[1]))
            {
                if (user_of_group(session_userid, tkns[1]))
                {
                    ack = handle_stop_share(tkns[1], tkns[2], session_userid);
                    return ack;
                }
                else
                {
                    // cout << "User not in group" << endl;
                    ack = "User not in group";
                }
            }
            else
            {
                // cout << "Group does not exist" << endl;
                ack = "Group does not exist";
            }
        }
        else
        {
            // cout << "User not logged in" << endl;
            ack = "User not logged in";
        }
    }

    //    show_downloads
    else
    {
        ack = "Not a Valid Command!!";
        return ack;
    }

    return ack;
}

void *handleclient(void *sd)
{
    int client_sd = *(int *)sd;
    char cmnd[buff_size] = {0};
    string usr = "";
    string skt = "";
    string ack;
    while (1)
    {
        // cout << "Waiting to Recieve Command..." << endl;
        int n = recv(client_sd, cmnd, buff_size, 0);

        if (n <= 0)
        {
            cout << "Error in receiving command" << endl;
            break;
        }

        cout << "Command received: " << cmnd << endl;

        string ack = cmmnd_parser(cmnd, client_sd, usr, skt);
        if (str_cmp(ack, "true"))
            continue;

        sendack(ack, client_sd);
        if (ack == "Connection Closed")
        {
            break;
        }
        cout << "------------------------------------------------" << endl;

        bzero(cmnd, buff_size);
    }
    close(client_sd);
    return NULL;
}

void run_server(int server_sd)
{
    // cout << "Inside run_server" << endl;
    sockaddr_in client_addr;
    socklen_t client_addr_size = sizeof(client_addr);

    // pthread_t thread_id[100];

    int i = 0;
    char cmnd[buff_size];
    bzero(cmnd, buff_size);
    pthread_t thread_id[100];
    while (1)
    {
        // cout << "Waiting for client...";
        int client_sd = accept(server_sd, (sockaddr *)&client_addr, &client_addr_size);
        cout << "Client connected " << endl;

        pthread_create(&thread_id[i++], NULL, handleclient, (void *)&client_sd);
    }

    for (int j = 0; j < i; j++)
    {
        pthread_join(thread_id[j], NULL);
    }
    close(server_sd);
}

void *server(void *p)
{
    // cout << "Inside Server" << endl;
    int port = *(int *)p;
    sockaddr_in server_addr;
    bzero((char *)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    int server_sd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_sd < 0)
    {
        cerr << "Cannot Acquire Socket" << endl;
        exit(0);
    }
    int bind_status = bind(server_sd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (bind_status < 0)
    {
        cerr << "Cannot Bind Socket" << endl;
        exit(0);
    }

    listen(server_sd, 5);

    run_server(server_sd);

    return NULL;
}

vector<string> extract_ip_port(string str)
{
    string delm = ":";
    vector<string> tokens;
    string token;

    for (unsigned i = 0; i < str.length(); i++)
    {
        if (str[i] == delm[0])
        {
            tokens.push_back(token);

            token = "";
        }
        else
        {
            token += str[i];
        }
    }
    tokens.push_back(token);

    return tokens;
}
void extract_port(vector<string> &tracker_info, string path)
{
    fstream file;
    file.open(path.c_str(), ios::in);
    string t;
    while (getline(file, t))
    {
        tracker_info.push_back(t);
    }
    file.close();
}

int main(int argc, char const *argv[])
{

    if (argc != 3)
    {
        cout << "Invalid number of arguments" << endl;
        return 0;
    }
    // string tracker_ip = argv[1];
    vector<string> tracker_details;
    string t_path = argv[1];
    // cout << "Tracker path: " << t_path << endl;
    extract_port(tracker_details, t_path);
    string tracker_ip = tracker_details[0];
    int positon = tracker_ip.find(":");
    string port = tracker_ip.substr(positon + 1);
    // cout << "Tracker IP: " << tracker_ip << " Port: " << port << endl;
    int listening_port = atoi(port.data());
    // int listening_port = atoi(argv[1]);
    pthread_t server_thread;

    // cout << "Inside Main";
    cout << "Tracker listening on port: " << listening_port << endl;
    pthread_create(&server_thread, NULL, server, &listening_port);

    string tracker_cmnd;
    while (1)
    {
        cin >> tracker_cmnd;
        if (tracker_cmnd == "quit")
        {
            exit(0);
        }
    }

    // pthread_join(server_thread, NULL);

    return 0;
}