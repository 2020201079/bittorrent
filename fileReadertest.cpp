#include<iostream>
#include<fstream>

using namespace std;

int main(){
    string myText;

    // Read from the text file
    ifstream MyReadFile("tracker_info.txt");

    getline(MyReadFile,myText);

    std::string port = myText.substr(myText.find_last_of(":") + 1);
    std::string ip = myText.substr(0,myText.find_last_of(":"));
    cout<<"IP is "<<ip<<endl;
    cout<<"port is "<<port<<endl;
    // Close the file
    MyReadFile.close();
}
