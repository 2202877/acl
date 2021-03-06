#include "acl_stdafx.hpp"
#include "acl_cpp/stdlib/snprintf.hpp"
#include "acl_cpp/stdlib/log.hpp"
#include "acl_cpp/stream/socket_stream.hpp"
#include "acl_cpp/session/memcache_session.hpp"
#include "acl_cpp/http/http_header.hpp"
#include "acl_cpp/http/HttpSession.hpp"
#include "acl_cpp/http/HttpServletRequest.hpp"
#include "acl_cpp/http/HttpServletResponse.hpp"
#include "acl_cpp/http/HttpServlet.hpp"

namespace acl
{

HttpServlet::HttpServlet(void)
{
	local_charset_[0] = 0;
	rw_timeout_ = 60;
	parse_body_enable_ = true;
	parse_body_limit_ = 102400;
}

HttpServlet::~HttpServlet(void)
{
}

HttpServlet& HttpServlet::setLocalCharset(const char* charset)
{
	if (charset && *charset)
		safe_snprintf(local_charset_, sizeof(local_charset_),
			"%s", charset);
	else
		local_charset_[0] =0;
	return *this;
}

HttpServlet& HttpServlet::setRwTimeout(int rw_timeout)
{
	rw_timeout_ = rw_timeout;
	return *this;
}

HttpServlet& HttpServlet::setParseBody(bool on)
{
	parse_body_enable_ = on;
	return *this;
}

HttpServlet& HttpServlet::setParseBodyLimit(int length)
{
	parse_body_limit_ = length;
	return *this;
}

bool HttpServlet::doRun(session& session, socket_stream* stream /* = NULL */)
{
	socket_stream* in;
	socket_stream* out;
	bool cgi_mode;

	if (stream == NULL)
	{
		// 数据流为空，则当 CGI 模式处理，将标准输入输出
		// 作为数据流
		stream = NEW socket_stream();
		(void) stream->open(ACL_VSTREAM_IN);
		in = stream;

		stream = NEW socket_stream();
		(void) stream->open(ACL_VSTREAM_OUT);
		out = stream;
		cgi_mode = true;
	}
	else
	{
		in = out = stream;
		cgi_mode = false;
	}

	// req/res 采用栈变量，减少内存分配次数

	HttpServletResponse res(*out);
	HttpServletRequest req(res, session, *in, local_charset_,
		parse_body_enable_, parse_body_limit_);

	if (rw_timeout_ >= 0)
		req.setRwTimeout(rw_timeout_);

	res.setCgiMode(cgi_mode);

	bool  ret;

	http_method_t method = req.getMethod();

	switch (method)
	{
	case HTTP_METHOD_GET:
		ret = doGet(req, res);
		break;
	case HTTP_METHOD_POST:
		ret = doPost(req, res);
		break;
	case HTTP_METHOD_PUT:
		ret = doPut(req, res);
		break;
	case HTTP_METHOD_CONNECT:
		ret = doConnect(req, res);
		break;
	case HTTP_METHOD_PURGE:
		ret = doPurge(req, res);
		break;
	case HTTP_METHOD_DELETE:
		ret = doDelete(req, res);
		break;
	case  HTTP_METHOD_HEAD:
		ret = doHead(req, res);
		break;
	case HTTP_METHOD_OPTION:
		ret = doOption(req, res);
		break;
	default:
		ret = false; // 有可能是IO失败或未知方法
		if (req.getLastError() == HTTP_REQ_ERR_METHOD)
			doUnknown(req, res);
		else
			doError(req, res);
		break;
	}

	if (in != out)
	{
		// 如果是标准输入输出流，则需要先将数据流与标准输入输出解绑，
		// 然后才能释放数据流对象，数据流内部会自动判断流句柄合法性
		// 这样可以保证与客户端保持长连接
		in->unbind();
		out->unbind();
		delete in;
		delete out;
	}

	return ret;
}

bool HttpServlet::doRun(const char* memcached_addr /* = "127.0.0.1:11211" */,
	socket_stream* stream /* = NULL */)
{
	memcache_session session(memcached_addr);
	return doRun(session, stream);
}

} // namespace acl
