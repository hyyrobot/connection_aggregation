#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H

#include <functional>
#include <list>

void cin_to_list(const std::function<void(std::list<std::string> &&)> &);

#endif // COMMAND_PARSER_H