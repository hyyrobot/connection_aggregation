#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H

#include <string>
#include <list>
#include <functional>

void cin_to_list(const std::function<void(std::list<std::string> &&)> &);

#endif // COMMAND_PARSER_H