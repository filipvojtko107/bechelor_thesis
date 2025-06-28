
// Connection preface send by web browser after TLS connection: 
// PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n


/*
// Ukladani binarnich  dat do stringu
#include <iostream>
#include <string>

int main() 
{
    // Binary data
    char binaryData[] = {static_cast<char>(0x00), (char)0xFF, (char)0xA5, (char)0x12};
    //std::string binaryString(reinterpret_cast<char*>(binaryData), sizeof(binaryData));
    std::string binaryString((char*) binaryData, sizeof(binaryData));

    // Output the binary string
    for (char c : binaryString) {
        printf("%02X ", static_cast<unsigned char>(c));
    }

    return 0;
}

*/