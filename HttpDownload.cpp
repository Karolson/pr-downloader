
#include "HttpDownload.h"
#include <stdio.h>
#include <curl/curl.h>
#include <unistd.h>
#include <string>
#include <string.h>
#include "FileSystem.h"
#include <zlib.h>
#include "Util.h"


CHttpDownload* CHttpDownload::singleton = NULL;


/** *
	draw a nice download status-bar
*/
int progress_func(void* ptr, double TotalToDownload, double NowDownloaded,
                    double TotalToUpload, double NowUploaded){
    // how wide you want the progress meter to be
    int totaldotz=40;
    double fractiondownloaded;
    if (TotalToDownload>0)
    	fractiondownloaded = NowDownloaded / TotalToDownload;
	else
		fractiondownloaded=0;
        // part of the progressmeter that's already "full"
    int dotz = fractiondownloaded * totaldotz;

    // create the "meter"
    printf("%5d/%5d ", httpDownload->getStatsPos(),httpDownload->getCount());
    printf("%3.0f%% [",fractiondownloaded*100);
    int ii=0;
    // part  that's full already
    for ( ; ii < dotz;ii++) {
        printf("=");
    }
    // remaining part (spaces)
    for ( ; ii < totaldotz;ii++) {
        printf(" ");
    }
    // and back to line begin - do not forget the fflush to avoid output buffering problems!
    printf("] %d/%d\r",(int)NowDownloaded,(int)TotalToDownload );
    fflush(stdout);
	return 0;
}


size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream) {
	int written = fwrite(ptr, size, nmemb, stream);
	return written;
}

bool CHttpDownload::download(const std::string& Url, const std::string& filename, int pos){
	CURLcode res=CURLE_OK;
    printf("Downloading %s to %s\n",Url.c_str(), filename.c_str());

	FILE* fp = fopen(filename.c_str() ,"wb+");
	if (fp<=NULL){
        printf("Could not open %s\n",filename.c_str());
		return false;
	}
	if(curl) {
		curl_easy_setopt(curl, CURLOPT_PROGRESSDATA , pos);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
		curl_easy_setopt(curl, CURLOPT_URL, Url.c_str());
		res = curl_easy_perform(curl);
	}
	fclose(fp);
	printf("\n"); //new line because of downloadbar
	if (res!=0){
		printf("error downloading %s\n",Url.c_str());
		unlink(filename.c_str());
		return false;
  }
  return true;
}

CHttpDownload::CHttpDownload(){
	curl_global_init(CURL_GLOBAL_ALL);
	curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, true);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, progress_func);
    stats_filepos=1;
    stats_count=1;
}

CHttpDownload::~CHttpDownload(){
	curl_easy_cleanup(curl);
	curl=NULL;
}

void CHttpDownload::Initialize(){
	singleton=new CHttpDownload();
}

void CHttpDownload::Shutdown(){
	delete(singleton);
	singleton=NULL;
}

void CHttpDownload::setCount(unsigned int count){
	this->stats_count=count;
}

unsigned int CHttpDownload::getCount(){
	return this->stats_count;
}

unsigned int CHttpDownload::getStatsPos(){
	return this->stats_filepos;
}

void CHttpDownload::setStatsPos(unsigned int pos){
	this->stats_filepos=pos;
}

std::list<CFileSystem::FileData*>::iterator list_it;
std::list<CFileSystem::FileData*>* globalFiles;
bool initialized;
FILE* file_handle;
std::string file_name;

unsigned int file_pos;
unsigned int skipped;
unsigned char cursize_buf[4];
unsigned int cursize;

unsigned int intmin(int x, int y){
	if(x<y)
		return x;
	return y;
}

static size_t write_streamed_data(const void* tmp, size_t size, size_t nmemb,void *userdata) {
	char buf[CURL_MAX_WRITE_SIZE];
	memcpy(&buf,tmp,CURL_MAX_WRITE_SIZE);
	if(!initialized){
		list_it=globalFiles->begin();
		initialized=true;
		file_handle=NULL;
		file_name="";
		skipped=0;
	}
	char* buf_start=(char*)&buf;
	const char* buf_end=buf_start + size*nmemb;
	char* buf_pos=buf_start;

	while(buf_pos<buf_end){ //all bytes written?
		if (file_handle==NULL){ //no open file, create one
			while( (!(*list_it)->download==true) && (list_it!=globalFiles->end())){ //get file
				list_it++;
			}
			file_name=fileSystem->getPoolFileName(*list_it);
			file_handle=fopen(file_name.c_str(),"wb");
			httpDownload->setStatsPos(httpDownload->getStatsPos()+1);
			if (file_handle==NULL){
				printf("couldn't open %s\n",(*list_it)->name.c_str());
				return -1;
			}
			//here comes the init new file stuff
			file_pos=0;
		}
		if (file_handle!=NULL){
			if((skipped>0)&&(skipped<4)){
//				printf("difficulty %d\n",skipped);
			}
			if (skipped<4){ // check if we skipped all 4 bytes, if not so, skip them
				int toskip=intmin(buf_end-buf_pos,4-skipped); //calculate bytes we can skip, could overlap received bufs
				for(int i=0;i<toskip;i++) //copy bufs avaiable
					cursize_buf[i]=buf_pos[i];
//				printf("toskip: %d skipped: %d\n",toskip,skipped);
				skipped=toskip+skipped;
				buf_pos=buf_pos+skipped;
				if (skipped==4){
					(*list_it)->compsize=parse_int32(cursize_buf);
				}
			}
			if (skipped==4){
				int towrite=intmin ((*list_it)->compsize-file_pos ,  //minimum of bytes to write left in file and bytes to write left in buf
					buf_end-buf_pos);
//				printf("%s %d %ld %ld %ld %d %d %d %d %d\n",file_name.c_str(), (*list_it)->compsize, buf_pos,buf_end, buf_start, towrite, size, nmemb , skipped, file_pos);
				int res=0;
				if (towrite>0){
					res=fwrite(buf_pos,1,towrite,file_handle);
					if (res!=towrite){
						printf("fwrite didn't write all\n");
					}
					if(res<=0){
						printf("\nwrote error: %d\n", res);
						return -1;
					}
				}

				buf_pos=buf_pos+res;
				file_pos+=res;
				if (file_pos>=(*list_it)->compsize){ //file finished -> next file
					fclose(file_handle);
					if (!fileSystem->fileIsValid(*list_it,file_name.c_str())){
						printf("File is broken?!: %s\n",file_name.c_str());
						return -1;
					}
					file_handle=NULL;
					list_it++;
					file_pos=0;
					skipped=0;
				}
			}
		}
	}
	return buf_pos-buf_start;

}

/**
	download files streamed
	streamer.cgi works as follows:
	* The client does a POST to /streamer.cgi?<hex>
	  Where hex = the name of the .sdp
	* The client then sends a gzipped bitarray representing the files
	  it wishes to download. Bitarray is formated in the obvious way,
	  an array of characters where each file in the sdp is represented
	  by the (index mod 8) bit (shifted left) of the (index div 8) byte
	  of the array.
	* streamer.cgi then responds with <big endian encoded int32 length>
	  <data of gzipped pool file> for all files requested. Files in the
	  pool are also gzipped, so there is no need to decompress unless
	  you wish to verify integrity. Note: The filesize here isn't the same
	  as in the .sdp, the sdp-file contains the uncompressed size
	* streamer.cgi also sets the Content-Length header in the reply so
	  you can implement a proper progress bar.

T 192.168.1.2:33202 -> 94.23.170.70:80 [AP]
POST /streamer.cgi?652e5bb5028ff4d2fc7fe43a952668a7 HTTP/1.1..Accept-Encodi
ng: identity..Content-Length: 29..Host: packages.springrts.com..Content-Typ
e: application/x-www-form-urlencoded..Connection: close..User-Agent: Python
-urllib/2.6....
##
T 192.168.1.2:33202 -> 94.23.170.70:80 [AP]
......zL..c`..`d.....K.n/....
*/
void CHttpDownload::downloadStream(std::string url,std::list<CFileSystem::FileData*>& files){
	CURL* curl;
	CURLcode res;
	curl = curl_easy_init();
	initialized=false;
	if(curl) {
		printf("Downloading stream: %s\n",url.c_str());

		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

		std::list<CFileSystem::FileData*>::iterator it;
		int  buflen=files.size()/8;
		if (files.size()%8!=0)
			buflen++;
		char* buf=(char*)malloc(buflen); //FIXME: compress blockwise and not all at once
		memset(buf,0,buflen);
		int destlen=files.size()*2;
		printf("%d %d %d\n",(int)files.size(),buflen,destlen);
		int i=0;
		for(it=files.begin();it!=files.end();it++){
			if ((*it)->download==true)
				buf[i/8] = buf[i/8] + (1<<(i%8));
			i++;
		}
		char* dest=(char*)malloc(destlen);

		gzip_str(buf,buflen,dest,&destlen);

		FILE* f;
		f=fopen("request","w");
		fwrite(buf, buflen,1,f);
		fclose(f);

		f=fopen("request.gz","w");
		fwrite(dest, destlen,1,f);
		fclose(f);

		free(buf);

		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_streamed_data);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &files);
		globalFiles=&files;
		curl_easy_setopt(curl, CURLOPT_FAILONERROR, true);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, dest);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,destlen);
		curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
		curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, progress_func);

		res = curl_easy_perform(curl);
		printf("\n"); //new line because of progressbar
		if (res!=CURLE_OK){
			printf("%s\n",curl_easy_strerror(res));
		}
		free(dest);
		/* always cleanup */
		curl_easy_cleanup(curl);
  }
}
