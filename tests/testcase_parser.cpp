#include <bits/stdc++.h>
#include <sstream>

using namespace std;

void read_instance(string instance_name) {
    int n_jobs, n_machines;
    {
        string line;
        vector<string> token_vec;
        while(getline(cin,line)) {
            stringstream tokens(line);
            string token;
            while(tokens >> token) {
                token_vec.push_back(token);
            }

            if (token_vec.size() == 2) {
                cout << "It has " << token_vec[0] << " Jobs and " << token_vec[1] << " Machines" << endl;
                n_jobs = stoi(token_vec[0]);
                n_machines = stoi(token_vec[1]);
                break;
            } else token_vec.clear();
        }
    }

    //put in a separate folder for organization
    ofstream testcase("./cases/"+instance_name+".txt");
    if (testcase.is_open()) cout << "Opened " << instance_name+".txt" << " successfully" << endl;
    testcase << n_jobs << ' ' << n_machines << '\n';
    cout << "It has " << n_jobs << " Jobs and " << n_machines << " Machines" << endl;
    string line;
    for (int i = 0; i < n_jobs; ++i) {
        getline(cin,line);
        stringstream tokens(line);
        string token;
        for (int j = 1; j <= n_machines; ++j) {
            // machine number
            tokens >> token;
            testcase << token << " ";
            // processing time
            tokens >> token;
            testcase << token << (j < n_machines ? " " : "");
        }
        testcase << "\n";
    }

    testcase << endl;
    testcase.close();
}

int main() {
    ios_base::sync_with_stdio(false);
    cin.tie(nullptr);

    string line;
    while(getline(cin,line)) {
        //cout << "test" << endl;
        stringstream tokens(line);
        string token;

        tokens >> token;

        if (token == "instance") {
            tokens >> token;
            cout << "Reading Instance " << token << endl;
            read_instance(token);
        }
    }
}
