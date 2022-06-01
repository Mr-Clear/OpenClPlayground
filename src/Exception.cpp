#include "Exception.h"

Exception::Exception(const std::string &message) :
    std::exception(),
    m_message(message)
{ }

const char *Exception::what() const noexcept
{
    return m_message.c_str();
}
