#include <iostream>

extern "C" {
	#include <libavformat/avformat.h>
	#include <libavutil/avutil.h>
}
void printUsage() {
	std::cout << "Usage: \n"
			<< "\t avSplit [filename]"
			<< std::endl;
}
#define RETURN_FATAL -1
#define RETURN_USER_ERROR 1
#define RETURN_SUCCESS 0


int main(int argc, char* argv[]) {
	if(argc < 2) {
		printUsage();
		return RETURN_USER_ERROR;
	}
	
	return RETURN_SUCCESS;
}