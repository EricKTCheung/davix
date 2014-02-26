#include "davmeta.hpp"
#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <deque>
#include <xml/davpropxmlparser.hpp>
#include <logger/davix_logger_internal.h>
#include <request/httprequest.hpp>
#include <fileops/fileutils.hpp>
#include <fileops/davops.hpp>
#include <utils/davix_utils_internal.hpp>
#include <string_utils/stringutils.hpp>
#include <xml/metalinkparser.hpp>


namespace Davix{

namespace Meta{


static const std::string propfind_request_replicas("<D:propfind xmlns:D=\"DAV:\" xmlns:L=\"LCGDM:\">"
                                                   "<D:prop><L:replicas/></D:prop>"
                                                   "</D:propfind>");


int davix_metalink_header_parser(const std::string & header_key, const std::string & header_value,
                                 const Uri & u_original,
                                 Uri & metalink){
    DAVIX_TRACE("Parse headers for metalink %s %s", header_key.c_str(), header_value.c_str());

    if(string_compare_ncase(header_key, "Link") ==0 && header_value.find("application/metalink") != std::string::npos){
        std::string::const_iterator it1, it2;
        if( ( it1 = std::find(header_value.begin(), header_value.end(), '<')) != header_value.end()
                && ( it2 = std::find(it1, header_value.end(), '>')) != header_value.end()){
            std::string metalink_url(it1+1, it2);
            metalink =  Uri::fromRelativePath(u_original, trim<int (*)(int)>(metalink_url));
            if(metalink.getStatus() == StatusCode::OK){
                DAVIX_TRACE("Valid metalink URI found %s", metalink.getString().c_str());
                return 1;
            }

        }
    }
    return 0;
}

bool davix_metalink_header_content_type(const std::string & header_key, const std::string & header_value){
    return (string_compare_ncase(header_key, "Content-type") ==0 &&  header_value.find("application/metalink") !=std::string::npos);
}

int davix_get_metalink_url( Context & c, const Uri & uri,
                            const RequestParams & _params, Uri & metalink){
    DavixError* tmp_err = NULL;
    RequestParams params(_params);
    // don't follow redirect, we need headers
    params.setTransparentRedirectionSupport(false);
    HeadRequest req(c, uri, &tmp_err);
    req.setParameters(params);
    req.addHeaderField("Accept", "application/metalink4+xml");


    DAVIX_TRACE("Executing head query to %s for Metalink file", uri.getString().c_str());
    if(tmp_err != NULL || (req.executeRequest(&tmp_err) <0))
        throw DavixException(davix_scope_meta(), tmp_err->getStatus(), tmp_err->getErrMsg());

    HeaderVec headers;
    req.getAnswerHeaders(headers);
    for(HeaderVec::iterator it = headers.begin(); it != headers.end(); it++){
        if( davix_metalink_header_parser(it->first, it->second, uri, metalink) > 0)
            return 1;

        if(davix_metalink_header_content_type(it->first, it->second)){
            // is a metalink content type, get it
            metalink = uri;
            return 1;
        }
    }




    return 0;
}


int davix_file_get_metalink_to_vfile(Context & c, const Uri & metalink_uri,
                                     const RequestParams & _params, std::vector<DavFile> & vec){
    DavixError * tmp_err=NULL;
    GetRequest req(c, metalink_uri, &tmp_err);
    MetalinkParser parser(c, vec);

    req.setParameters(_params);
    req.addHeaderField("Accept", "application/metalink4+xml");

    DAVIX_TRACE("Executing query for %s Metalink content", metalink_uri.getString().c_str());
    if(tmp_err != NULL || (req.beginRequest(&tmp_err) <0) )
        throw DavixException(davix_scope_meta(), tmp_err->getStatus(), tmp_err->getErrMsg());
    if(httpcodeIsValid(req.getRequestCode()) == false){
        std::ostringstream ss;
        ss << "Unable to get Metalink file, error HTTP " << req.getRequestCode();
        throw DavixException(davix_scope_meta(), StatusCode::InvalidServerResponse, ss.str());
    }

    dav_ssize_t read_size;
    do{
        char buffer[2049];
        buffer[2048] = '\0';
        if( (read_size = req.readSegment(buffer, 2048, &tmp_err)) < 0)
            throw DavixException(davix_scope_meta(), tmp_err->getStatus(), tmp_err->getErrMsg());
        parser.parseChuck(buffer, read_size);
    }while(read_size > 0);

    req.endRequest(NULL);
    return vec.size();
}

int davix_file_get_all_replicas_metalink( Context & c, const Uri & uri,
                                 const RequestParams & _params, std::vector<DavFile> & vec){
    Uri metalink;
    if(davix_get_metalink_url(c,  uri,
                              _params, metalink) > 0
            && davix_file_get_metalink_to_vfile(c, metalink,_params, vec) > 0){
        return static_cast<int>(vec.size());

    }
    throw DavixException(davix_scope_meta(), StatusCode::OperationNonSupported, "Server does not support Metalink standard");
    return 0;
}


// get all reps from webdav queries
void getReplicas(Context & c, const Uri & uri,
                              const RequestParams & params,
                                std::vector<DavFile> & vec){
    davix_file_get_all_replicas_metalink(c, uri, params, vec);
}



/**
  execute a propfind/stat request on a given HTTP request handle
  return a vector with the content of the request if success
*/
const char* req_webdav_propfind(HttpRequest* req, DavixError** err){
    DavixError* tmp_err=NULL;
    int ret =-1;

    req->addHeaderField("Depth","0");
    req->setRequestMethod("PROPFIND");
    if( (ret = req->executeRequest(&tmp_err)) ==0){
        ret = davixRequestToFileStatus(req, davix_scope_stat_str(), &tmp_err);
    }

    if(ret != 0)
        DavixError::propagateError(err, tmp_err);

    return req->getAnswerContent();
}


int dav_stat_mapper_webdav(Context &context, const RequestParams* params, const Uri & url, struct stat* st,
                           DavixError** err){
    int ret =-1;

    DavPropXMLParser parser;
    DavixError * tmp_err=NULL;
    HttpRequest req(context, url, &tmp_err);
    if( tmp_err == NULL){
        req.setParameters(params);

        TRY_DAVIX{
            const char * res = req_webdav_propfind(&req, &tmp_err);
            if(!tmp_err){
               parser.parseChuck((const char*) res, strlen(res));

                std::deque<FileProperties> & props = parser.getProperties();
                if( props.size() < 1){
                    throw DavixException(davix_scope_stat_str(), Davix::StatusCode::WebDavPropertiesParsingError, "Parsing Error : properties number < 1");
                }else{
                    fill_stat_from_fileproperties(st, props.front());
                    ret =0;
                }
            }
        }CATCH_DAVIX(&tmp_err)
        if(tmp_err != NULL)
            ret = -1;
    }
    if(tmp_err)
        DavixError::propagateError(err, tmp_err);
    return ret;
}


int dav_stat_mapper_http(Context& context, const RequestParams* params, const Uri & uri, struct stat* st,
                         DavixError** err){
    int ret = -1;
    DavixError * tmp_err=NULL;
    HeadRequest req(context, uri, &tmp_err);

    if( tmp_err == NULL){
        req.setParameters(params);
        req.executeRequest(&tmp_err);

        if(!tmp_err){
            if(httpcodeIsValid(req.getRequestCode()) ){
                memset(st, 0, sizeof(struct stat));
                const dav_ssize_t s = req.getAnswerSize();
                st->st_size = static_cast<size_t>(std::max<dav_ssize_t>(0,s));
                st->st_mode = 0755 | S_IFREG;
                ret = 0;
            }else{
                httpcodeToDavixCode(req.getRequestCode(), davix_scope_http_request(), uri.getString() , &tmp_err);
                ret = -1;
            }
        }
    }
    if(tmp_err)
        DavixError::propagateError(err, tmp_err);
    return ret;
}


dav_ssize_t posixStat(Context & c, const Uri & url, const RequestParams * params,
                      struct stat* st, DavixError** err){
    RequestParams _params(params);
    DavixError* tmp_err=NULL;
    int ret =-1;
    configureRequestParamsProto(url, _params);

    switch(_params.getProtocol()){
         case RequestProtocol::Webdav:
            ret = dav_stat_mapper_webdav(c, &_params, url, st, &tmp_err);
            break;
        default:
            ret = dav_stat_mapper_http(c, &_params, url, st, &tmp_err);
            break;

    }
    DAVIX_DEBUG(" davix_stat <-");
    if(tmp_err)
        DavixError::propagatePrefixedError(err, tmp_err, "stat ops : ");
    return ret;
}

int deleteResource(Context & c, const Uri & url, const RequestParams & params, DavixError** err){
    DavixError* tmp_err=NULL;
    int ret=-1;
    RequestParams _params(params);
    configureRequestParamsProto(url, _params);

    DeleteRequest req(c,url, err);
    req.setParameters(_params);
    if(!tmp_err){
        ret=req.executeRequest(&tmp_err);
        if(!tmp_err && httpcodeIsValid(req.getRequestCode()) == false){
                httpcodeToDavixCode(req.getRequestCode(), davix_scope_stat_str(), url.getString() , &tmp_err);
                ret = -1;
         }
    }


    if(tmp_err)
        DavixError::propagateError(err, tmp_err);
    return ret;
}


int makeCollection(Context & c, const Uri & url, const RequestParams & params, DavixError** err){
    DAVIX_DEBUG(" -> makeCollection");
    int ret=-1;
    DavixError* tmp_err=NULL;
    RequestParams _params(params);
    configureRequestParamsProto(url, _params);

    HttpRequest req(c, url, &tmp_err);

    if(tmp_err == NULL){
        req.setParameters(params);
        req.setRequestMethod("MKCOL");
        if( (ret = req.executeRequest(&tmp_err)) == 0){
            ret = davixRequestToFileStatus(&req, davix_scope_mkdir_str(), &tmp_err);
        }

        DAVIX_DEBUG(" makeCollection <-");
    }
    if(tmp_err)
        DavixError::propagateError(err, tmp_err);
    return ret;
}

} // Meta

} // Davix
