#include <iostream>
#include <fstream>
#include <iterator>
#include <exception>
#include <stdio.h>
#include <stdlib.h>

#include <stdlib.h>
#include <stdio.h>

#include <termios.h>
#include <string.h>
#include <sys/ioctl.h>

#include <sys/time.h>

#include <list>

// libtorrent needs boost, anyway
// so why not use it ourselves?
#include <boost/filesystem.hpp>

#include "libtorrent/entry.hpp"
#include "libtorrent/bencode.hpp"
#include "libtorrent/session.hpp"


float difftime( const struct timeval& from, const struct timeval& to ) {
    return (to.tv_sec - from.tv_sec) + ( to.tv_usec - from.tv_usec ) / 1000000.0f;
}

int main(int argc, char* argv[])
{


    using namespace libtorrent;
#if BOOST_VERSION < 103400
    namespace fs = boost::filesystem;
    fs::path::default_name_check(fs::no_check);
#endif

    //handles_t handles;
    struct timeval tstart, tend;
    gettimeofday( &tstart, NULL );
    if (argc > 8 || argc < 2)
    {
        std::cerr << "usage: ./simple_client [-s:-o:-n:-d] torrent-file\n\t\t"
                                "\t\t-s  seeder: don't exist after download finishes\n"
                                "\t\t-o  download directory\n"
                                "\t\t-n  number of upload slots\n"
                                "\t\t-d  directory containing torrent files to seed\n"
                                "\t\t-q  use sequential piece picker\n"
                                ;
        return 1;
    }

    std::string save_path("./");
    std::list<std::string> torrents;
    bool seeder = false;
    int slots = 4;
    bool sequential = false;

    for (int i=1; i<argc; ++i)
    {
        if (argv[i][0] != '-')
            torrents.push_back(std::string(argv[i]));
        else
        {
            switch (argv[i][1])
            {
                case 's': 
                    seeder = true;
                    break;
                case 'o':
                    i++;
                    if( i < argc )
                        save_path = argv[i];
                    else {
                        std::cerr << "-o requires an argument\n";
                        return -1;
                    }
                    break;
                case 'n':
                    i++;
                    if( i < argc )
                        slots = atoi(argv[i]);
                    else {
                        std::cerr << "-n requires an argument\n";
                        return -1;
                    }
                    break;
                case 'd':
                    i++;
                    if( i < argc ) {
                        DIR* d = opendir( argv[i] );
                        if( !d )
                            std::cerr << "Could not open dir " << argv[i] << "\n";
                        else {
                            struct dirent* e;
                            while( e = readdir( d ) ) {
                                if( (!strcmp( e->d_name, "." )) || (!strcmp( e->d_name, ".." )) )
                                    continue;
                                std::string p( argv[i] );
                                p = p + "/" + e->d_name;
                                torrents.push_back(p);
                            }
                            closedir( d );
                        }
                    }
                    else {
                        std::cerr << "-d requires an argument\n";
                        return -1;
                    }
                    break;
                case 'q':
                    sequential = true;
                    break;
                default:
                    std::cerr << "Unknown option: " << argv[i] << "\n";
                    return -1;
            }
        }
    }
   
    if (save_path == "./")
        std::cerr << "Warning: missing download directory param\n";

    if( torrents.size() == 0 ) {
        std::cerr << "No torrents found\n";
        return -1;
    }

    session s;
   
    // check if we have to set a random port
    srand((unsigned)time(0));
    int random_integer;
    int lowest=6666, highest=8888;
    int range=(highest-lowest)+1;
    int listen_port = lowest+int(range*rand()/(RAND_MAX + 1.0));

    s.listen_on(std::make_pair(lowest, highest));
    s.set_max_uploads(30);

    std::list<torrent_handle> handles;
   
    for( std::list<std::string>::iterator i = torrents.begin(); i != torrents.end(); i++ ) {
        std::string& torrent = *i;
        add_torrent_params p;
        p.save_path = save_path;
        error_code ec;
        p.ti = new torrent_info(torrent, ec);
        if (ec)
        {
            std::cout << ec.message() << std::endl;
            return 1;
        }
        torrent_handle h = s.add_torrent(p, ec);
        // set sequential dl
        if( sequential )
            h.set_sequential_download(true);
        if (ec)
        {
            std::cerr << ec.message() << std::endl;
            return 1;
        }
        handles.push_back(h);
    }

    //session_status sess_stat = s.status();
    //torrent_status ts = h.status();
    //main loop... print usefull data logs
    long int down = 0;
    long int up = 0;
    float progress = 0.0;
    fprintf(stderr,"time\t%%complete\tup B\tdown B\n");
    while (true)
    {
        gettimeofday(&tend, NULL);
        long int newup = s.status().total_upload;
        long int newdown = s.status().total_download;
        up = newup - up;
        down = newdown - down;
        bool allComplete = true;
        progress = 0.0;
        for( std::list<torrent_handle>::iterator i = handles.begin(); i != handles.end(); i++ ) {
            torrent_handle& h = *i;
            progress += h.status().progress_ppm;
            if(h.status().state != 5)
                allComplete = false;
        }
        progress /= handles.size();
       
        float dtime = difftime(tstart, tend);
        // %f for time, since 100 times "2 seconds" is not very useful
        // %0.2f for progress, since sub-percentage progress is available and hence more accurate; no more precision than 2 since at that point libtorrent itself
        //       doesn't seem that accurate (without actual progress the progress alternatingly increased and decreased less than 0.01 percent)
        // %li for up and down since that's in bytes
        fprintf(stderr,"%f\t%0.2f\t%li\t%li\n",
         dtime,
         (float)(progress / 10000.f),
         (long int)up,
         (long int)down);
        
        up = newup;
        down = newdown;
       
        // check if complete
        if (allComplete && !seeder)
            return 0;
        // This sleep is actually not the usual Linux sleep, but the one defined in libtorrent, which takes usecs
        // Going for 999 usec to compensate a little for execution time.
        sleep(999);
    }

    return 0;
}
