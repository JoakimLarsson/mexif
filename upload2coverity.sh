cov-build --dir cov-int gcc -o mexif exif.c
tar czvf mexif.tgz cov-int
curl --form project=JoakimLarsson%2FMexif --form token=dnyk45R8rWT_YYk6je76HA --form email=joakim@bildrulle.nu --form file=@./mexif.tgz --form version="0.x" --form description="Another build"  https://scan.coverity.com/builds?project=JoakimLarsson%2FMexif
