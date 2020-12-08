#include <iostream>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>

#include "src/config_reader.h"
int main(int argc,char** argv){
	FILE * config;
	int ii;
	char line[100];
	config = fopen(argv[1], "r");
	assert(config != NULL && "Could not open config file");
	ii = 0;
	std::string str;
	while (fscanf(config, "%s\n", line) != EOF) {
		str.append(line);
	}
	std::cerr << str << "\n";
	ConfigReader msg;
	msg.setJson(str);
	std::cerr << msg.getValue("cache_size_l1").asInt() << "\n";
}
