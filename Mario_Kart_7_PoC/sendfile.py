#!/usr/bin/env Python

import sys
import ftplib
import datetime
from ftplib import FTP

# Usage:
# sendfile.py "filename" "ftppath" "host" "ip"

def printf(string):
    print(datetime.datetime.strftime(datetime.datetime.now(), '%Y-%m-%d %H:%M:%S') + " : " + string);

def checkfolder(ftp):
    files = []

    try:
        files = ftp.nlst()
    except ftplib.error_perm as resp:
        if str(resp) != "550 No files found":
            printf("Couldn't retrieve file list")
        else:
            return

    for f in files:
        if (".3dsxg" in f):# or "CTRPFData.bin" in f):
            parts = f.split()
            name = parts[len(parts) - 1]
            printf("Deleting " + name)
            try:
                ftp.delete(name)
                printf(name + " was successfully deleted")
            except Exception:
                printf("An error occured")
                continue


if __name__ == '__main__':
    print("");
    printf("FTP File Sender\n")
    try:
        filename = sys.argv[1]
        path = sys.argv[2]
        host = sys.argv[3]
        port = sys.argv[4]
        destfile = sys.argv[5]

        ftp = FTP()
        printf("Connecting to " + host + ":" + port);
        ftp.connect(host, int(port));
        printf("Connected");

        printf("Opening " + filename);
        file = open(sys.argv[1], "rb");
        printf("Success");

    except IOError as e:
        printf("/!\ An error occured. /!\ ");

    printf("Moving to: ftp:/" + path);
    try:
        ftp.cwd(path);
    except IOError:
        try:
            ftp.mkd(path);
            ftp.cwd(path);
        except Exception:
            pass

    checkfolder(ftp)

    try:        
        printf("Sending file");
        ftp.storbinary('STOR '+ destfile, file);
        printf("Done")

        file.close();

        ftp.quit();
        printf("Disconnected");

    except Exception:
        printf("/!\ An error occured. /!\ ");
