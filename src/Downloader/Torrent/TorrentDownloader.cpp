#include <string>
#include "TorrentDownloader.h"

#include "libtorrent/entry.hpp"
#include "libtorrent/bencode.hpp"
#include "libtorrent/session.hpp"
#include <libtorrent/alert_types.hpp>
#include "FileSystem.h"
#include "Util.h"

bool CTorrentDownloader::download(const std::string& torrentfile, const std::string& filename){
	printf("%s %s:%d \n",__FILE__, __FUNCTION__ ,__LINE__);
	return true;
}

void CTorrentDownloader::start(IDownload* download){
	printf("%s %s:%d \n",__FILE__, __FUNCTION__ ,__LINE__);
	if (download==NULL)
		return;
	libtorrent::session s;

//	s=new libtorrent::session();

    libtorrent::session_settings setting;
    setting.tracker_completion_timeout=1;
    setting.stop_tracker_timeout=1;
    setting.peer_timeout=1;
    setting.urlseed_timeout=1;

	s.set_settings(setting);

	s.listen_on(std::make_pair(6881, 6889));

	libtorrent::add_torrent_params p;
	p.save_path = download->name; //name contains the path, because torrents already include the filenames
    p.ti = new libtorrent::torrent_info(download->url.c_str());
	for (int i=0; i<p.ti->num_files(); i++){
		printf("File %d in torrent: %s\n",i, p.ti->file_at(i).path.filename().c_str());
	}


    printf("Downloading torrent to %s\n", download->name.c_str());
    libtorrent::torrent_handle tor=s.add_torrent(p);

    std::list<std::string>::iterator it;
	for(it=download->mirror.begin(); it!=download->mirror.end(); it++){
		printf("Adding webseed to torrent %s\n",(*it).c_str());
		urlEncode(*it);
    	tor.add_url_seed(*it);
	}
	libtorrent::torrent_info info = tor.get_torrent_info();

    while( (!tor.is_seed()) && (tor.is_valid())){
		const libtorrent::session_status& sessinfo=s.status();
		printf("\r%ld/%ld",sessinfo.total_download, info.total_size());
		fflush(stdout);
		libtorrent::time_duration time(1000000); //1 sec
		const libtorrent::alert* a = s.wait_for_alert(time);
		if (a!=NULL){
			printf("peer error: %d %d\n",a->category(),libtorrent::alert::peer_notification | libtorrent::alert::error_notification );

			if (( a->category() &  libtorrent::alert::peer_notification) &&
				(  a->category() & libtorrent::alert::error_notification)){
				printf("wrong peer found!\n");
			}
			printf("%s\n",a->message().c_str());
			s.pop_alert();
		}
    }

    printf("download finished, shuting down torrent...\n");
    s.pause();
	printf("shut down!\n");

}

const IDownload* CTorrentDownloader::addDownload(const std::string& url, const std::string& filename){
	printf("%s %s:%d \n",__FILE__, __FUNCTION__ ,__LINE__);
	return NULL;
}

bool CTorrentDownloader::removeDownload(IDownload& download){
	printf("%s %s:%d \n",__FILE__, __FUNCTION__ ,__LINE__);
	return true;
}

std::list<IDownload>* CTorrentDownloader::search(const std::string& name){
	printf("%s %s:%d \n",__FILE__, __FUNCTION__ ,__LINE__);
	return NULL;
}
