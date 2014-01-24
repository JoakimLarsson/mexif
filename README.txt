mexif
=====

A fast and minimal exif library

The project started in 2007 when I saw that the imageviewer gThumb in the Gnome 
desktop on my Linux computer trashed the EXIF data of my JPEG images. Or rather
the proprietary EXIF data added by Nikon. So I dived in and found no C library
that could fix faulty EXIF tags, like the wrong date/time and the orientation
when viewing a rotated image. 

So I started to dig in to the EXIF standard and wrote a small patch to the gThumb 
project which allowed gThumb to modify the date through a dialog. It worked great 
for me even in bulk mode, so I could add dates to my newly scanned photo negatives.
I also made a patch to fix the orientation EXIF data after a rotation using the
same routines.

At the time the routine was much faster then the available EXIF hacks in C++, Perl
and Python that was available then and it also preserved the private EXIF data
instead of trashing it like the libexif did. The patched was accepted by the gThumb
prject somewhere in 2007.

I also wrote a special EXIF thumbnail extractor for the Gnome desktop itself and it 
was accepted by proxy through the gThumb maintainers. Great fun and a good insight 
in the world of OSS contributers in really large projects.

Joakim Larsson

