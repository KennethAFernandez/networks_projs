// **************************************************************************************
// * Echo Strings (echo_s.cc)
// * -- Accepts TCP connections and then echos back each string sent.
// **************************************************************************************
#include "echo_s.h"
#include <cstring>
#include <iostream>
#include <regex>


void sig_handler(int signo) {
	DEBUG << "Caught signal #" << signo << ENDL;	
	DEBUG << "Closing file descriptors 3-31" << ENDL;
	for (int i = 0; i < 32; i++ ) close(i);
	exit(1);
}	

/**
 *	readRequest(int socketFD, std::string *filename){
 *	default return code 400
 *	 	 	read everything up to and including the end of the header
 *	look at first line of header for valid GET
 *	 	if there is valid GET, find filename
 *	 	if filename, make sure it is valid
 *	 		if filename valid, set return code 200
 *			if invalid return code 404
**/

int readRequest(int socketFD, std::string *filename) {
	
	int rc = 400;
	char buf[1024];
	bool get;
	bool valid;

	std::string hdr = "";
	std::regex e ("GET\\s+/(file\\d\\.html)|(image\\d\\.jpg)\\s+HTTP/\\d\\.\\d\r\n");	
	std::regex e2 ("(file\\d\\.html|image\\d\\.jpg)");
	std::smatch sm;

	// read in and find header
	DEBUG << "readRequest()" << ENDL;	
	ssize_t in = read(socketFD, buf, 1024);
	for(int i = 0; i < in; i++){
		if (buf[i] == '\r' && buf[i+1] == '\n') { // && buf[i+2] == '\r' && buf[i+3] == '\n') {
			hdr += "\r\n\r\n";
			break;
		} 
		hdr += buf[i];

	}
	DEBUG<<"HDR: "<<hdr<<ENDL;

	DEBUG << "Checking for valid GET" << ENDL;
	get = regex_search(hdr, sm, e);
	// check if valid get, if so then check if valid name
	if (get) {
		DEBUG << "Valid GET" << ENDL;
		DEBUG << "Checking if filename is valid" << ENDL;

		valid = regex_search (hdr, sm, e2);
		*filename = (std::string)sm[0];
		std::cout<<*filename << std::endl;

		if (valid) {
			rc = 200;
		} else {
			rc = 404;
		}
	}
	return rc;
}

/**
 *	void sendLine(int socketFD, std::string stringToSend){
 *	covert string to array that is 2 bytes longer than the string
 *	replace last two bytes with the <CR> and <LF>
 *	use write to send that array
**/

void sendLine(int socketFD, std::string stringToSend) {

	DEBUG << "SENDLINE" << ENDL;	

	char arr[stringToSend.length() + 2];
	std::strcpy(arr, stringToSend.c_str());
	arr[stringToSend.length()] = '\r';
	arr[stringToSend.length()+1] = '\n';
	write(socketFD, arr, stringToSend.length() + 2);
}

/**
 *	void send404(int socketFD){
 *	using sendline() function
 *		send properly formatted http response with 404 code
 *		send string, "content-type: text/html"
 *		send blank line to terminate the header
 *		send friendly message about problem
 *		send blank line to end
**/
 
void send404(int socketFD) {

	DEBUG << "SEND404" <<ENDL;

	sendLine(socketFD, "HTTP/1.0 404 Not Found");
	sendLine(socketFD, "Content-type: text/html");
	sendLine(socketFD, "\n");
	sendLine(socketFD, "[FILE NOT FOUND] 404");
	sendLine(socketFD, "\n");	
}

/**
 *	void send400(int socketFD);
 *	using sendLine()
 *		properly formatted http
 *		blank line
**/

void send400(int socketFD) {

	DEBUG << "SEND400" <<ENDL;

	sendLine(socketFD, "HTTP/1.0 400 Bad Request");
	sendLine(socketFD, "Content-type: text/html");
	sendLine(socketFD, "\n");
	sendLine(socketFD, "[BAD REQUEST] 400");
	sendLine(socketFD, "\n");
}

/**
 *	sendFile(socketFD, filename);
 *	stat() func to find size of the file
 *		if fail, or no read persmission , send 404
 *	sendLine() to send the header
 *		formatted http with code 200
 *		send content type
 *		send content-length
 *	send file itself
 *		open
 *		allocate 10 bytes with malloc or new[]
 *		in a loop:
 *			clear memoty with bzero
 *			read() up to 10 bytes from file into buffer
 *			wirte() # of bytes you read
 *		just return when done
**/

void sendFile(int socketFD, std::string filename) {

	const char *c = filename.c_str();
	struct stat s;
	int check = stat(c, &s);
	ssize_t sz = s.st_size;

	DEBUG << "sendFile()" << ENDL;
	// check stat() func to find size of the file,
	// if issue, send404()
	// if no issue find file tpye and send html

	if(check < 0) {
		send404(socketFD);
		return;
	} else {
		std::string type = filename.substr(filename.rfind('.'), filename.length());
		std::string sol = "";
		if (type == ".jpg") { sol = "image/jpeg"; }
		if (type == ".html") { sol = "text/html"; }
		sendLine(socketFD, "HTTP/1.0 200 OK");	
		sendLine(socketFD, "Content-Type: " + sol);
		sendLine(socketFD, "Content-Length: " + std::to_string(sz));
		sendLine(socketFD, "");
	}


	// open file as read only, allocate space of buffer
	// while the size is greater than zero, write bytes
	// subtract from sz of file
	int file = open(filename.c_str(), O_RDONLY);
	ssize_t bz = 512;
	ssize_t grab;
	char buf[bz];
	while(sz > 0) {
		//size_t grab = sz >= bz ? bz : sz;
		if (sz >= bz) {
			grab = bz;
		} else {
			grab = sz;
		}

		ssize_t reads= read(file, buf, grab);

		write(socketFD, buf, reads);
		sz -= reads;
	}
//	ssize_t bz = 1024;
//	char buf[bz];
//	for (int i = 0; i<sz; i++){
//		fread(buf, 1, sz, file);
//		write(socketFD, buf, buf[i]);
//	}
	DEBUG << "FINISHED" << ENDL;
}

// **************************************************************************************
// * processConnection()
// * - Handles reading the line from the network and sending it back to the client.
// * - Returns 1 if the client sends "QUIT" command, 0 if the client sends "CLOSE".
// **************************************************************************************
int processConnection(int sockFd) {

//  int quitProgram = 0;
 // int keepGoing = 1;
  //char buf[1024];
  //while (keepGoing) {
  	
    //
    // Call read() call to get a buffer/line from the client.
    // Hint - don't forget to zero out the buffer each time you use it.
    //
    //	DEBUG << "Calling read("<<sockFd<<","<<&buf<< ","<< "1024" << ")"<< ENDL;
//	ssize_t readVar = read(sockFd, buf, 1024);
//	buf[readVar - 1] = '\0'; 
//	readVar += 1;

	std::string filename = "";
	int rc = readRequest(sockFd, &filename);
	DEBUG << "RC: " << rc <<ENDL;
	DEBUG << "filename: " << filename << ENDL;

	if (rc == 404) {
		DEBUG << "404" <<ENDL;
		send404(sockFd); 
	} else if (rc == 400) { 
		DEBUG << "400" << ENDL;
		send400(sockFd); 
	} else if (rc == 200) { 
		DEBUG << "200" <<ENDL;
		sendFile(sockFd, filename); 
	}
	DEBUG << "RETURN AND WAIT" << ENDL;
    //
    // Check for one of the commands
    //
	
	// Check if input is QUIT, if so return 1 and quit
//	if (std::strcmp(buf, "QUIT") == 0){
//		quitProgram = 1;
//		keepGoing = 0;
//		return quitProgram;
//	}
	// Check if input is CLOSE, if so return 0 and wait for new connection
//	if (std::strcmp(buf, "CLOSE") == 0){
//		quitProgram = 0;
//		keepGoing = 0;
//		return quitProgram;
//	}

//	DEBUG << "Received " << readVar << " bytes, contaning the string " << buf << ENDL;
    //
    // Call write() to send line back to the client.
    //
    //	buf[readVar - 1] = '\n';
  //  	DEBUG << "Calling write(" << sockFd << "," <<&buf << "," << readVar << ")" << ENDL;
//	ssize_t writeVar = write(sockFd, buf, readVar);

//	DEBUG << "Wrote " << readVar << " back to client." << ENDL;

//	int close(sockFd);

  return 0
  ;
}
    


// * - Sets up the sockets and accepts new connection until processConnection() returns 1
// **************************************************************************************

int main (int argc, char *argv[]) {

  // ********************************************************************
  // * Process the command line arguments
  // ********************************************************************
  boost::log::add_console_log(std::cout, boost::log::keywords::format = "%Message%");
  boost::log::core::get()->set_filter(boost::log::trivial::severity >= boost::log::trivial::info);

  // ********************************************************************
  // * Process the command line arguments
  // ********************************************************************
  int opt = 0;
  while ((opt = getopt(argc,argv,"v")) != -1) {
    
    switch (opt) {
    case 'v':
      boost::log::core::get()->set_filter(boost::log::trivial::severity >= boost::log::trivial::debug);
      break;
    case ':':
    case '?':
    default:
      std::cout << "useage: " << argv[0] << " -v" << std::endl;
      exit(-1);
    }
  }

  // *******************************************************************
  // * Creating the inital socket is the same as in a client.
  // ********************************************************************
  int     listenFd = -1;
  // Call socket() to create the socket you will use for lisening.
  // Creates socket, with domain and type, checks if fails
  DEBUG << "Calling Socket() assigned file descriptor " << listenFd << ENDL;
  if ((listenFd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	  std::cout<< "Failed to create listening socket " << strerror(errno) << std::endl;
	  exit(-1);
  }

  
  // ********************************************************************
  // * The bind() and calls take a structure that specifies the
  // * address to be used for the connection. On the cient it contains
  // * the address of the server to connect to. On the server it specifies
  // * which IP address and port to lisen for connections.
  // ********************************************************************
  struct sockaddr_in servaddr;
  srand(time(NULL));
  int port = (rand() % 10000) + 1024;
  bzero(&servaddr, sizeof(servaddr));
  servaddr.sin_family = PF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons(port);


  // ********************************************************************
  // * Binding configures the socket with the parameters we have
  // * specified in the servaddr structure.  This step is implicit in
  // * the connect() call, but must be explicitly listed for servers.
  // ********************************************************************
  DEBUG << "Calling bind(" << listenFd << "," << &servaddr << "," << sizeof(servaddr) << ")" << ENDL;
  int bindSuccesful = 0;
  //Binds the socket to an address and port number
  while (!bindSuccesful) {
    // You may have to call bind multiple times if another process is already using the port
    // your program selects.
    if (bind(listenFd, (sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
	    std::cout << "bind() failed: " << strerror(errno) << std::endl;
    }else {
	    bindSuccesful = 1;
    }
  }
  std::cout << "Using port " << port << std::endl;


  // ********************************************************************
  // * Setting the socket to the listening state is the second step
  // * needed to being accepting connections.  This creates a queue for
  // * connections and starts the kernel listening for connections.
  // ********************************************************************
  int listenQueueLength = 1;
  // Creates listening queue and associate it with the socket
  DEBUG << "Calling listen(" << listenFd << "," << listenQueueLength << ")" << ENDL;
  if (listen(listenFd, listenQueueLength) < 0) {
	  std::cout << "listen() failed: " << strerror(errno) << std::endl;
	  exit(-1);
  }

  // ********************************************************************
  // * The accept call will sleep, waiting for a connection.  When 
  // * a connection request comes in the accept() call creates a NEW
  // * socket with a new fd that will be used for the communication.
  // ********************************************************************
  int quitProgram = 0;
  // Accepts call after connection
  while (!quitProgram) {
    int connFd = 0;
    DEBUG << "Calling accept(" << listenFd << "NULL,NULL)." << ENDL;
    if ((connFd = accept(listenFd, (sockaddr *) NULL, NULL)) < 0){
	    std::cout<< "accept() failed: " << strerror(errno) << std::endl;
	    exit(-1);
    }
	
    // The accept() call checks the listening queue for connection requests.
    // If a client has already tried to connect accept() will complete the
    // connection and return a file descriptor that you can read from and
    // write to. If there is no connection waiting accept() will block and
    // not return until there is a connection.
    
    DEBUG << "We have recieved a connection on " << connFd << ENDL;

    
    quitProgram = processConnection(connFd);
    
    close(connFd);
  }

  close(listenFd);

  DEBUG << "QUIT" << ENDL;

}
// **************************************************************************************
// * Echo Strings (echo_s.cc)
