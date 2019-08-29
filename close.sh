sudo netstat -lpn | grep $1
sudo fuser -k $1/tcp
