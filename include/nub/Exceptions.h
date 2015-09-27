/*  <nub/Exceptions.h> -- Exceptions thrown by NUB
    Copyright (c) 2005-2009 by Gerald Lindsly

    See <nub/Platform.h> for additional copyright information
*/
#ifndef __NUB_EXCEPTIONS_H__
#define __NUB_EXCEPTIONS_H__

#include <stdexcept>
#include <string>

namespace nub {

using namespace std;

class nub_exception : public exception
{
public:
	explicit nub_exception(const string& _Message)
		: _Str(_Message)
	{}

	virtual ~nub_exception() throw()
	{}

	virtual const char* what() const throw()
    {
        return _Str.c_str();
	}

private:
	string _Str;	// the stored message string
};


class io_error : public nub_exception
{
public:
	explicit io_error(const string& _Message)
        : nub_exception("io error: " + _Message)
	{}
};


} // namespace nub

#endif // __NUB_EXCEPTIONS_H__
