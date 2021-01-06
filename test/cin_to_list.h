#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H

#include <string>
#include <list>
#include <functional>
#include <istream>

void stream_to_list(std::istream &, const std::function<bool(std::list<std::string> &&)> &);

#endif // COMMAND_PARSER_H