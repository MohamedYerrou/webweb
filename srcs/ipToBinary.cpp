#include  <iostream>
#include  <string>
#include  <sstream>
// #include <netinet/in.h>
#include <arpa/inet.h>

// 127.0.0.1

bool	is_number(std::string& nbr)
{
	size_t	i = 0;
	while (i < nbr.size())
	{
		if (isdigit(nbr[i++]) == 0)
			return false;
	}
	return true;
}

unsigned int	ip_toBinary(std::string str)
{
	std::stringstream	ss1(str);
	std::string			word;
	int					nbr = 0;
	int					iter = 3;


	while (std::getline(ss1, word, '.'))
	{
		if (!is_number(word))
			return -1;
		std::stringstream	ss2(word);
		int					tmp_nbr = 0;
		ss2 >> tmp_nbr;
		if (is_number(word) && (tmp_nbr < 0 || tmp_nbr > 255))
			return -1;
		nbr  = nbr | tmp_nbr;
		if (iter--)
			nbr = nbr << 8;
	}
	return htonl(nbr);
}

int	main()
{
	unsigned int i = 1;
	i = i << 8;
	// std::cout << i << std::endl;
	unsigned char* c = (unsigned char *)&i;
	// if (*c == 0)
	// 	std::cout << "Big endian" << std::endl;
	// else if (*c == 1)
	// 	std::cout << "Little endian" << std::endl;
	// else
	// 	std::cout << "something else" << std::endl;
	std::cout << ip_toBinary("255.300.255.255") << std::endl;
	std::cout << inet_addr("255.255.255.255") << std::endl;
	return 0;
}
