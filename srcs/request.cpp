#include "request.hpp"

Request::~Request()
{ }

Request::Request(char *raw)
{
	this->r_full_request = raw;
	std::istringstream iss(raw);

	iss>> this->r_method>> this->r_location >> this->r_version;
}
Request::Request(char *raw, const ServerConfig &servr)
{
	this->r_full_request = raw;
	std::istringstream iss(raw);

	// std::cout << this->r_full_request << std::endl;
	iss>> this->r_method>> this->r_location >> this->r_version;
	check_allowed_methods(servr);
}



void Request::check_allowed_methods(const ServerConfig &server)
{
	std::vector<LocationConfig>::const_iterator it_loc = server.locations.begin();
    for(;it_loc != server.locations.end();it_loc++)
    {
		//std::cout << it_loc->path << " vs " <<  this->r_location << std::endl;
		if(it_loc->path == this->r_location)
		{
			//std::cout << "found" << std::endl;
			this->_loc = *it_loc;
			std::vector<std::string>::const_iterator it_meth = it_loc->allowed_methods.begin();
			for(;it_meth != it_loc->allowed_methods.end();it_meth++)
			{
				//std::cout << *it_meth << std::endl;
			    if(this->r_method == *it_meth)
			    {
			        this->authorized = true;
                    this->execute(); // <--- then execute it
					return ;
			    }
			}
			return ;
		}
    }
	this->execute(); // <--- then execute it
}
// ________________EXECUTE METHOD____________________
void Request::execute()
{
	std::cout<<"\033[48;5;236mREQUEST = '" << this->r_location<<"' ";
	if(!this->authorized)
	{
		this->Get();

	}
	else if (this->r_method == "GET")
		this->Get();
	else if (this->r_method == "POST")
		this->Post();

}



// ________________POST METHOD____________________
void Request::Post()
{
	std::cout<<"POST | EXECUTED !!> \033[0m"<<std::endl;
	// this->authorized =false;
}
// ______________________________________________





// ______________________GET METHOD____________________________
static std::string readFile(const std::string& file_path)
{
    std::ifstream file(file_path.c_str());
    if (!file.is_open())
        return ""; // cant open file

    std::stringstream buffer;
    buffer << file.rdbuf();  // Read entire file contents into buffer
    return buffer.str();     // Return as a std::string
}
void Request::Get()
{
    std::cout << "|GET EXECUTED !!| \033[0m" << std::endl;

    std::string full_path = this->_loc.root + "/" + this->_loc.index;
    std::string file_path;

	
    struct stat st;
    if (stat(full_path.c_str(), &st) == 0)
    {
		std::cout << "cqcq" << std::endl;
        if (S_ISDIR(st.st_mode))
        {
			std::cout << "45498748" << std::endl;
			std::cout << "A" << std::endl;
            // check all  index files
            for (unsigned long i = 0; i < this->_loc.index_files.size(); i++)
            {
                std::string index_path = full_path;
            	//if (!full_path.empty() && full_path[full_path.size() - 1] != '/')
                index_path += "/";
                index_path += this->_loc.index_files[i];

                // Check if index file exists and is readable
                if (access(index_path.c_str(), R_OK) == 0)
                {
                    file_path = index_path; // Found valid index file
                }
            }

            // no index file found, check autoindex
            if (this->_loc.autoindex)
            	file_path = "[AUTOINDEX]"; // special marker handled elsewhere
            else
            	file_path = "404"; // no autoindex and no index file => 404 scenario
        }
        else
        {
			std::cout << "" << std::endl;
            // Regular file
            file_path = full_path;
        }
    }
	else
		file_path = "[WRONGPATH]";

	std::cout << file_path << std::endl;
	// 404
	if (file_path.empty() || file_path == "404")
    {
        // actually hadnl 404 error
		const std::string response =
			"HTTP/1.1 404 Not Found\r\n"
			"Content-Type: text/html\r\n"
			"Content-Length: 134\r\n"
			"Connection: close\r\n"
			"\r\n"
			"<html>\r\n"
			"<head><title>WEBSERV - 404</title></head>\r\n"
			"<body>\r\n"
			"<h1>ERROR 404</h1>\r\n"
			"<h2>wbserv</h2>\r\n"
			"<h2>cheh</h2>\r\n"
			"<p>The requested URL was found on this server </p>\r\n"
			"</body>\r\n"
			"</html>";
		this->_ReqContent = (response);
    }
    //  autoindex
    else if (file_path == "[WRONGPATH]" )
    {
        // generate autoindex HTML or handle accordingly
        // --> todo return generateAutoindexHtml();
		        // actually hadnl 404 error
		std::ostringstream oss;
		oss << "HTTP/1.1 404 Not Found\r\n"
			<< "Content-Type: text/html\r\n"
			<< "Connection: close\r\n"
			<< "\r\n"
			<< "<html>\r\n"
			<< "<head><title>WEBSERVER || AUTOINDEX</title></head>\r\n"
			<< "<body>\r\n"
			<< "<h1>Bonjour mais malheureusement.</h1>\r\n"
			<< "<h1>Non.</h1>\r\n"
			<< "<h2>PATH couldn't be found so I render you a default page bro</h2>\r\n"
			<< "<h2>the path was " << full_path << "</h2>\r\n"
			<< "</body>\r\n"
			<< "</html>";

		std::string response = oss.str();
		this->_ReqContent = (response);
    }else if (file_path == "[AUTOINDEX]")
	{
		const std::string response =
			"HTTP/1.1 404 Not Found\r\n"
			"Content-Type: text/html\r\n"
			"Content-Length: 134\r\n"
			"Connection: close\r\n"
			"\r\n"
			"<html>\r\n"
			"<head><title>WEBSERV - AUTOINDEX</title></head>\r\n"
			"<body>\r\n"
			"<h2>cheh</h2>\r\n"
			"</body>\r\n"
			"</html>";
		this->_ReqContent = (response);
	}
	// default file (like index.html)
	else
	{
		std::cout << "3" << std::endl;
		const std::string& 
			body = readFile(file_path),
			contentType = "text/html";

		// return file content in a sort of http kind idk
		std::stringstream response;
		response << "HTTP/1.1 200 OK\r\n";
		response << "Content-Type: " << contentType << "\r\n";
		response << "Content-Length: " << body.size() << "\r\n";
		response << "Connection: close\r\n";
		response << "\r\n"; // End of headers
		response << body;
		this->_ReqContent = ( response.str());
	}
	std::cout << "\033[1;48;5;236m"<< this->_ReqContent << "\033[0m"<< std::endl;
}

std::string Request::_get_ReqContent()
{
	return this->_ReqContent;
}
// ______________________________________________
