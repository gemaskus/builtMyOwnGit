#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>
#include <zlib.h>
#include <vector>
#include <openssl/sha.h>
#include <algorithm>

#define TREE_TYPE_OBJECT  40000
#define BLOB_TYPE_OBJECT 100644
#define EXE_TYPE_OBJECT  100755
#define SYMB_LINK_OBJECT 120000

bool hashObject(std::string file);
std::string decompressFile(std::string filePath);
bool ls_tree(std::string treeSHA, bool nameOnly);
std::string shaFile(std::string fileName);
std::string write_tree(std::string treeFilePath);

int main(int argc, char *argv[])
{
    // Flush after every std::cout / std::cerr
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    //You can use print statements as follows for debugging, they'll be visible when running tests.
    //std::cout << "Logs from your program will appear here!\n";

    if (argc < 2) {
        std::cerr << "No command provided.\n";
        return EXIT_FAILURE;
    }
    
    std::string command = argv[1];
    
    if (command == "init") {
        try {
            std::filesystem::create_directory(".git");
            std::filesystem::create_directory(".git/objects");
            std::filesystem::create_directory(".git/refs");
    
            std::ofstream headFile(".git/HEAD");
            if (headFile.is_open()) {
                headFile << "ref: refs/heads/main\n";
                headFile.close();
            } else {
                std::cerr << "Failed to create .git/HEAD file.\n";
                return EXIT_FAILURE;
            }
    
            std::cout << "Initialized git directory\n";
        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << e.what() << '\n';
            return EXIT_FAILURE;
        }
    }
    else if(command == "cat-file")
    {
        if(argc <= 3){
            std::cerr<<"Invalid number of args \n";
            return EXIT_FAILURE;
        }
        
        const std::string flag = argv[2];
        
        if(flag != "-p"){
            std::cerr<<"Invalid flag for cat-file command\n";
            return EXIT_FAILURE;
        }
        
        const std::string blobName = argv[3];
        const std::string dirName = blobName.substr(0,2);
        const std::string blobSha = blobName.substr(2);

        std::string filePath = ".git/objects/"+dirName+"/"+blobSha;
        
        std::string buf = decompressFile(filePath);

        if(buf == ""){
            return EXIT_FAILURE;
        }

        const std::string objectContent = buf.substr(buf.find('\0')+1);

        std::cout<<objectContent<<std::flush;
    }
    else if(command == "hash-object")
    {
        if(argc <= 3){
            std::cerr<<"Invalid number of args \n";
            return EXIT_FAILURE;
        }
        std::string hash = argv[3];

        if(!hashObject(hash)){
            return EXIT_FAILURE;
        }

    }
    else if(command == "ls-tree"){
        if(argc <= 2){
            std::cerr<<"Invalid number of args \n";
        }else if (argc == 3){
            std::string treeSHAString = argv[2];
            if(!ls_tree(treeSHAString, false)){
                return EXIT_FAILURE;
            }

        }else if (argc == 4){
            std::string flag = argv[2];

            if(flag != "--name-only"){
                std::cerr<<"Invalid flag for ls-tree command, expected '--name-only'\n";
                return EXIT_FAILURE;
            }
            std::string treeSHAString = argv[3];
            if(!ls_tree(treeSHAString, true)){
                return EXIT_FAILURE;
            }
        }
    }else if(command == "write-tree"){
        if(argc >2){
            std::cerr<<"Invalid number of arguments for write-tree, expected 2 got: "<< std::to_string(argc)<<"\n";
            return EXIT_FAILURE;
        }
        //get the current working directory to give to the write-tree function
        std::string currentWorkingDirectory = std::filesystem::current_path();
        
        //get the SHA based on the current tree
        std::string treeSHA = write_tree(currentWorkingDirectory);
        
        //print out the treeSHA
        std::cout<<treeSHA<<"\n"

    }else {
        std::cerr << "Unknown command " << command << '\n';
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}

bool hashObject(std::string file){
    std::ifstream input(file);

    if(!input.is_open()){
        std::cerr<<"Unable to open given file: "<<file<<std::endl;
        return false;
    }
    std::stringstream fileData;

    fileData << input.rdbuf();

    std::string content = "blob "+std::to_string(fileData.str().length())+'\0'+fileData.str();

    unsigned char hash[20];

    SHA1((unsigned char*)content.c_str(), content.size(), hash);

    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for(const auto& byte : hash){
        ss << std::setw(2) << static_cast<int>(byte);
    }
    std::cout << ss.str() << std::endl;

    std::string fileSha = ss.str();

    ulong bound = compressBound(content.size());

    unsigned char compressedData[bound];

    compress(compressedData, &bound, (const Bytef *)content.c_str(), content.size());

    std::string newDir = ".git/objects/"+fileSha.substr(0,2);
    std::filesystem::create_directory(newDir);
    std::string blobObjectPath = newDir+"/"+fileSha.substr(2);
    std::ofstream blobObjectFile(blobObjectPath, std::ios::binary);
    blobObjectFile.write((char*)compressedData, bound);
    blobObjectFile.close();
    return true;
}

bool ls_tree(std::string treeSHA, bool nameOnly){

    const std::string dirName = treeSHA.substr(0,2);
    const std::string treeSha = treeSHA.substr(2);

    std::string filePath = ".git/objects/"+dirName+"/"+treeSha;

    std::string treeFileContents = decompressFile(filePath);
    
    //std::cout<<treeFileContents<<std::endl;

    std::vector<std::string> treeObjects;

    std::string line;

    std::string trimmedContents = treeFileContents.substr(treeFileContents.find('\0')+1);
    do{ 
        line = trimmedContents.substr(0,trimmedContents.find('\0'));
        std::string tempString = line.substr(0, 5);
        uint32_t typeNum = std::stoi(tempString);
        std::string objectName;

        if(typeNum == TREE_TYPE_OBJECT){
            objectName = line.substr(6);
        }else{
            objectName = line.substr(7);
        }
        treeObjects.push_back(objectName);
    
        trimmedContents = trimmedContents.substr(trimmedContents.find('\0')+21);
    }while(trimmedContents.size()>1);

    sort(treeObjects.begin(), treeObjects.end());
    
    for(int i = 0; i<treeObjects.size(); i++){
        std::cout<<treeObjects[i]<<std::endl;
    }
    return true;
}

std::string decompressFile(std::string filePath){
    auto in = std::ifstream(filePath);

    if(!in.is_open()){
        std::cerr<<"Failed to open object file\n";
    };

    const auto objectStr = std::string(std::istreambuf_iterator<char>(in),
                           std::istreambuf_iterator<char>());
        
    in.close();

    std::string buf;
    buf.resize(objectStr.size());
    while(true){
        auto len = buf.size();
        int res = uncompress((uint8_t*)buf.data(), &len, (const uint8_t*)objectStr.data(), objectStr.size());
        if(res == Z_BUF_ERROR){
            buf.resize(buf.size()*2);
        }else if(res != Z_OK){
            return "";
        }else{
            buf.resize(len);
            break;
        }
    }

    return buf;
}

std::string shaFile(std::string filePath, bool isTree){
    std::ifstream()
}

std::string write_tree(std::string treeFilePath){
    std::vector treeContents <std::string>;
    for(const auto& entry : std::filesystem::directory_iterator(treeFilePath)){
        int mode = 0; // 0 = tree, 1 = blob, 2 = 
        //get the filename string of the current entry in the tree
        const auto filePathString = entry.path().string();
        const auto filenameString = entry.path().filename().string();
        std::string fileSHAString;
        if(entry.is_directory()){
            //if the current entry is a directory, recursively get this 
            write_tree(filePathString);
        }else if(entry.is_regular_file){
            fileSHAString = shaFile(filePathString, false);
        }


    }



    return "";
}