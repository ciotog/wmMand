#include <stdio.h>
#include <sys/file.h>
#include <sys/types.h>
#include <stdlib.h>
#include <dirent.h>
#include <malloc.h>
#define TRUE 1
#define FALSE 0
#define MAX_FILES 30
#define NITMAX 11  /* means that 65536 will be the max number of its */
static int iter_opts[] = {64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536};
static int size_opts[] = {100, 200 ,300, 400, 500, 600};
static double zoom_opts[] = {0.01, 0.1, 0.25, 0.5, 0.75, 
			     1.0, 2.0, 4.0, 6.0, 8.0, 10.0, 
			     12.0, 14.0, 16.0, 18.0, 20.0};

int DEBUG= 0; /* used in xvgif.c */

main(argc, argv)
int argc;
char *argv[];
{
	double a, b, c, d, im, re, val, f, a2, b2;
	double new_range, xp, yp, nrange;
	double old_range, old_x, old_y, zoom;
	double jold_range, jold_cx, jold_cy, jold_x, jold_y;
	char newfile[256], oldfile[256], ctab[20], ctab_file[256];
	char newjfile[256], oldjfile[256], last_Mimage[80], last_Jimage[80];
	register int n;
	int i, j, old_map_size, map_size, dummy, max_iterations, jold_map_size;
	int fd, in_set, Image_x, Image_y, nmin, nmax, hist[65536], ncolors;
	int ncount, count, done, cutoff, sflg, new_num, jnew_num;
	unsigned char *image;
	int *tmp_image;
	FILE *fp, *fp_gif, *fpp;
        char query_string[512], field[21][50], *clen, ctab_opts[20][50];
	char tmpfile[256], do_julia[10];
        double RANGE, CX, CY, X, Y, hmp, ohmp;
	unsigned char red[256], grn[256], blu[256], uval;
	int R, G, B, content_length, ni, nf, len;
	int ctab_fileselect(), jfileselect(), fileselect(), alphasort(), compar(), jcompar();
	int nctabfiles, njfiles, nfiles, last_num, jlast_num, julia_posted, post_julia;
	struct dirent **namelist, **jnamelist;
	struct dirent **ctablist;
	char line[1050];

	/* for debugging purposes */
	/*
	FILE *fphist;
	fphist=fopen("/u/mgh/public_html/frac/junk/histogram.dat", "w");
	*/





	/* read in the stuff from stdin and decode it */
	clen = getenv("CONTENT_LENGTH");
	if (clen == NULL){
		printf("Content-Type: text/html\n");
/*
		fpp = fopen("/n/toaster/u/mgh/public_html/mand.shtml", "r");
		while (fgets(line, 1024, fpp) != NULL){
			printf("%s", line);
		}
*/
		printf("Location: http://nis-www.lanl.gov/~mgh/mand.shtml\n\n");
		exit(1);
	}
	content_length = atoi(clen);

	fscanf(stdin, "%s", query_string);



	for (i=0, nf=0, ni=0; i<content_length; ++i){
		if (query_string[i] == '&'){
			/* terminate string first */
			field[nf][ni] = '\0';
			++nf;
			ni = 0;
		}
		else{
			field[nf][ni] = query_string[i];
			++ni;
		}
	}

	/* if sflg == 0 then we are working from the main_mand gif */
	sscanf(field[0],  "sflg=%d", &sflg);

	/* this is the range of the previous Mset image */
	sscanf(field[1],  "or=%lf", &old_range);

	/* this is the center point of the previous Mset image */
	sscanf(field[2],  "ox=%lf", &old_x);
	sscanf(field[3],  "oy=%lf", &old_y);

	/* this is map size of the previous Mset image */
	sscanf(field[4],  "oms=%d", &old_map_size);

	/* this is the mandelbrot map that was generated last */
	sscanf(field[5],  "last_Mimage=%s", last_Mimage);




	/* this is the range of the previous Jset image */
	sscanf(field[6],  "jor=%lf", &jold_range);

	/* this is the center point of the previous Jset image */
	sscanf(field[7],  "jox=%lf", &jold_x);
	sscanf(field[8],  "joy=%lf", &jold_y);

	/* this is the parameter c of the previous Jset image */
	sscanf(field[9],  "jocx=%lf", &jold_cx);
	sscanf(field[10],  "jocy=%lf", &jold_cy);

	/* this is map size of the previous Jset image */
	sscanf(field[11],  "joms=%d", &jold_map_size);

	/* this is the julia map that was generated last */
	sscanf(field[12],  "last_Jimage=%s", last_Jimage);


	/* this indicates whether a Jset map is up already */
	sscanf(field[13],  "julia_posted=%d", &julia_posted);
	sscanf(field[14],  "Size=%dx%d",    &map_size, &dummy);
	sscanf(field[15],  "Iterations=%d", &max_iterations);
	sscanf(field[16],  "Zoom=%lf", &zoom);
	sscanf(field[17],  "Ctab=%s", ctab);
	sscanf(field[18],  "Julia=%s", do_julia);

	/* check to see if max_iterations is reasonable -- dont want to
	   chew up CPU cycles on errors or perhaps malicious users */
	if (max_iterations > iter_opts[NITMAX-1])
		exit(-1);



	/* in the following section we try to figure out what the user
	   is trying to do. And we then set the parameters needed for that 
	   operation. Note that X,Y values always refer to the center of the
	   map being worked on. CX,CY refer to the c parameter needed in the
	   Julia set iterations. */

	if ((!strcmp(do_julia, "yes"))&&(julia_posted == 0)){ 
		/* then there is currently no julia map posted 
		   so the user must have clicked on the Mset map.
                   Find x,y of clicked point and use as c for Jset. */

		sscanf(field[19], "Mset.x=%d", &Image_x);
		sscanf(field[20], "Mset.y=%d", &Image_y);

		/* since this is a new Jset fix RANGE at 2.0 to staert out */
		RANGE = 2.0;

		/* compute the (X, Y) values of the point defined by (Image_x,Image_y)
 			   to get the paramter c */
		ohmp = (double)old_map_size/2.0;
		hmp = (double)map_size/2.0;
		CX = old_range * ((double)Image_x-ohmp)/ohmp + old_x; 
		CY = old_range * ((double)Image_y-ohmp)/ohmp + old_y;

		/* make the center of the map (0,0) */
		X = 0.0;
		Y = 0.0;

		/* set flag to 1 -- indicating that we are going to add a julia map */
		post_julia = 1;
	}
	else if ((!strcmp(do_julia, "yes"))&&(julia_posted == 1)){
		/* then there are currently an Mset AND a Jset map posted.
		   Either way we are going to update the Jset map. BUT
 		   if the user clicked in the Mset map then we do a whole new
		   Jset whereas if they clicked in the Jset map then we will
		   just zoom into the Jset map. */
		if (!strncmp(field[19], "Mset", 4)){
			sscanf(field[19], "Mset.x=%d", &Image_x);
			sscanf(field[20], "Mset.y=%d", &Image_y);

			/* since this is a new Jset fix RANGE at 2.0 to staert out */
			RANGE = 2.0;
	
			/* compute the (X, Y) values of the point defined by (Image_x,Image_y) to
			   to get the paramter c */
			ohmp = (double)old_map_size/2.0;
			hmp = (double)map_size/2.0;
			CX = old_range * ((double)Image_x-ohmp)/ohmp + old_x; 
			CY = old_range * ((double)Image_y-ohmp)/ohmp + old_y;

			/* make the center of the map (0,0) */
			X = 0.0;
			Y = 0.0;
		}
		else{
			sscanf(field[19], "Jset.x=%d", &Image_x);
			sscanf(field[20], "Jset.y=%d", &Image_y);

			RANGE = jold_range/zoom;
	
			/* compute the (X, Y) values of the point defined by (Image_x,Image_y) */
			hmp = (double)map_size/2.0;
			ohmp = (double)jold_map_size/2.0;
			X = jold_range * ((double)Image_x-ohmp)/ohmp + jold_x; 
			Y = jold_range * ((double)Image_y-ohmp)/ohmp + jold_y;

			CX = jold_cx;
			CY = jold_cy;
		}
		post_julia = 1;
	}
	else{
		/* no julia set displayed - so just zoom into Mset map. */
		sscanf(field[19], "Mset.x=%d", &Image_x);
		sscanf(field[20], "Mset.y=%d", &Image_y);

		RANGE = old_range/zoom;

		/* compute the (X, Y) values of the point defined by (Image_x,Image_y) */
		ohmp = (double)old_map_size/2.0;
		hmp = (double)map_size/2.0;
		X = old_range * ((double)Image_x-ohmp)/ohmp + old_x; 
		Y = old_range * ((double)Image_y-ohmp)/ohmp + old_y;

		post_julia = 0;
	}
	
	
	if (!strcmp(do_julia, "no")){
		image = (unsigned char *)malloc(map_size*map_size*sizeof(unsigned char));
		tmp_image = (int *)malloc(map_size*map_size*sizeof(int));
	
		/* compute the (integer) map first */
		nmin = 9999, nmax = -9999, ncount = 0;
		f = RANGE/hmp;
		for (i=0; i<map_size; ++i){
			re = f * ((double)i-hmp) + X;
			for (j=0; j<map_size; ++j){
				im = f * ((double)j-hmp) + Y;
				n = 0;
				a = b = 0.0;
				a2 = 0.0;
				b2 = 0.0;
				while ((n < max_iterations)&&((a2 + b2) < 4.0)){
					d = a;

					a = a2 - b2 + re;
					b = 2.0*d*b + im;

					a2 = a*a;
					b2 = b*b;
					++n;
				}
				if (n > nmax) nmax = n;
				if (n < nmin) nmin = n;
				if (n == max_iterations){
					*(tmp_image + map_size*j + i) = 0;
				}
				else{
					*(tmp_image + map_size*j + i) = n;
					++ncount;
				}
				++hist[*(tmp_image + map_size*j + i)];
			}
			/*printf("i=%d\n", i);*/
		}


		/* figure out what nmax should be. The problem is that you might only get
                   one or two points in the upper half of the given range. A quick and dirty
                   method is to find out at what n the f(n) starts to just become outliers.
                   define outliers as mostly zeros. e.g. say more than 50% zero in a stretch
                   that is 10 contiguous values long. Or you could count backwards until you
                   get to the 99.5% level and call that the cutoff. */
		i = nmax-1, done = FALSE, count = 0;
		while((i > 0)&&(done == FALSE)){
			count += hist[i];
			if ((double)count/(double)ncount >= 0.005){
				done = TRUE;
				cutoff = i;
			}
			--i;
		}

	
		/* then map the integer map (tmp_image) into a byte map (i.e. with 256 colours) */
		nrange = (double)(cutoff-nmin)+1.0;
		for (i=0; i<map_size; ++i){
			for (j=0; j<map_size; ++j){
				n = *(tmp_image + map_size*j + i);
				if (n == 0){
					*(image + map_size*j + i) = 0;
				}
				else if (n >= cutoff){
					*(image + map_size*j + i) = 255;
				}
				else if (n != 0){
					/* make sure roundoff doesnt put index to 0. Otherwise it'll
				  	   look like its in the set! */
					uval = (unsigned char)(((double)(n-nmin)+1.0)/nrange * 255.);
					*(image + map_size*j + i) = (uval == 0) ? 1 : uval;
				}
			}
		}

	}
	else{ /* compute the julia set */
		image = (unsigned char *)malloc(map_size*map_size*sizeof(unsigned char));
		tmp_image = (int *)malloc(map_size*map_size*sizeof(int));
	
		/* compute the (integer) map first */
		nmin = 9999, nmax = -9999, ncount = 0;
		f = RANGE/hmp;
		for (i=0; i<map_size; ++i){
			re = f * ((double)i-hmp) + X; 
			for (j=0; j<map_size; ++j){
				im = f * ((double)j-hmp) + Y;

				a = re;
				b = im;

				n = 0;
				a2 = a*a;
				b2 = b*b;
				while ((n < max_iterations)&&((a2 + b2) < 4.0)){
					d = a;

					a = a2 - b2 + CX;
					b = 2.0*d*b + CY;

					a2 = a*a;
					b2 = b*b;

					++n;
				}
				if (n > nmax) nmax = n;
				if (n < nmin) nmin = n;
				if (n == max_iterations){
					*(tmp_image + map_size*j + i) = 0;
				}
				else{
					*(tmp_image + map_size*j + i) = n;
					++ncount;
				}
				++hist[*(tmp_image + map_size*j + i)];
			}
			/*printf("i=%d\n", i);*/
		}


		/* figure out what nmax should be. The problem is that you might only get
                   one or two points in the upper half of the given range. A quick and dirty
                   method is to find out at what n the f(n) starts to just become outliers.
                   define outliers as mostly zeros. e.g. say more than 50% zero in a stretch
                   that is 10 contiguous values long. Or you could count backwards until you
                   get to the 99.5% level and call that the cutoff. */
		i = nmax-1, done = FALSE;
		while((i > 0)&&(done == FALSE)){
			count += hist[i];
			if ((double)count/(double)ncount >= 0.005){
				done = TRUE;
				cutoff = i;
			}
			--i;
		}

	
		/* then map the integer map (tmp_image) into a byte map (i.e. with 256 colours) */
		nrange = (double)(cutoff-nmin)+1.0;
		for (i=0; i<map_size; ++i){
			for (j=0; j<map_size; ++j){
				n = *(tmp_image + map_size*j + i);
				if (n == 0){
					*(image + map_size*j + i) = 0;
				}
				else if (n >= cutoff){
					*(image + map_size*j + i) = 255;
				}
				else if (n != 0){
					/* make sure roundoff doesnt put index to 0. Otherwise it'll
				  	   look like its in the set! */
					uval = (unsigned char)(((double)(n-nmin)+1.0)/nrange * 255.);
					*(image + map_size*j + i) = (uval == 0) ? 1 : uval;
				}
			}
		}

	}



	/* now determine what colortables are present */
	nctabfiles = scandir("/u/mgh/public_html/frac/colortables", &ctablist, ctab_fileselect, alphasort);
	for (i=0; i<nctabfiles; ++i){
		strcpy(tmpfile, (*(ctablist + i))->d_name);
		len = strlen(tmpfile);
		for (j=0; j<len-4; ++j) ctab_opts[i][j] = tmpfile[j];
		ctab_opts[i][len-4] = '\0';
	}





	/* get the color table info */
	sprintf(ctab_file, "/u/mgh/public_html/frac/colortables/%s.dat", ctab);
	fp = fopen(ctab_file, "r");
	for (i=0; i<256; ++i){
		fscanf(fp, "%d %d %d", &R, &G, &B);
		red[i] = R, grn[i] = G, blu[i] = B;
	}
	fclose(fp);

	/* determine the file number that was used last for Mset images */
	nfiles = scandir("/u/mgh/public_html/frac/junk", &namelist, fileselect, compar);
	sscanf((*(namelist + 0))->d_name, "mandelbrot%d.gif", &last_num);
	new_num = last_num + 1;
		
	/* determine the file number that was used last for Jset images */
	njfiles = scandir("/u/mgh/public_html/frac/junk", &jnamelist, jfileselect, jcompar);
	sscanf((*(jnamelist + 0))->d_name, "julia%d.gif", &jlast_num);
	jnew_num = jlast_num + 1;

	/* Note that someone else could have created the lastnum file found above! 
	   Thats why we keep our own last num stored as a hidden variable */


	
	if (!strcmp(do_julia, "no")){
		if (nfiles > MAX_FILES){
			sprintf(oldfile, "/u/mgh/public_html/frac/junk/mandelbrot%d.gif", last_num-20);
			unlink(oldfile);
		}
		
		sprintf(newfile, "/u/mgh/public_html/frac/junk/mandelbrot%d.gif", new_num);
		fp_gif = fopen(newfile, "w");
		WriteGIF(fp_gif, image, 0, map_size, map_size, red, grn, blu, 256, 0, "");
		fclose(fp_gif);
	}
	else{
		if (njfiles > MAX_FILES){
			sprintf(oldjfile, "/u/mgh/public_html/frac/junk/julia%d.gif", jlast_num-20);
			unlink(oldjfile);
		}
		
		sprintf(newjfile, "/u/mgh/public_html/frac/junk/julia%d.gif", jnew_num);
		fp_gif = fopen(newjfile, "w");
		WriteGIF(fp_gif, image, 0, map_size, map_size, red, grn, blu, 256, 0, "");
		fclose(fp_gif);
	}
	


/*
	printf("Content-Type: text/html%c%c", 10, 10);
*/
printf("Content-Type: text/html\n");
printf("Location: http://nis-www.lanl.gov/~mgh/mandclosed.html\n\n");
exit(1);
	/*printf("QUERY_STRING: %s\n", query_string);*/
	printf("<html>\n");
	printf("<title>Mandelbrot/Julia Set Browser</title>\n");
	printf("<body bgcolor=\"#666666\" background =\"raised_spots2.jpg\" text=\"#00bbff\" link=\"#00ff99\" vlink=\"#a080ff\" alink=\"#ff0000\">\n");
	printf("<center><h1><b>Mandelbrot/Julia Set Browser</B></h1></center>\n\n");
	printf("<hr>\n\n");



	printf("<FORM METHOD=POST ACTION=\"/~mgh/mand.cgi\">\n");
	if (!strcmp(do_julia, "no")){
		printf("<INPUT TYPE=\"image\" NAME=\"Mset\" SRC=\"/~mgh/frac/junk/mandelbrot%d.gif\" align=left vspace=10 hspace=5> </a> \n", new_num);
	}
	else if (sflg == 0){
		printf("<INPUT TYPE=\"image\" NAME=\"Mset\" SRC=\"/~mgh/frac/junk/main_mand.gif\" align=left vspace=10 hspace=5> </a> \n");
	}
	else {
		printf("<INPUT TYPE=\"image\" NAME=\"Mset\" SRC=\"/~mgh/frac/junk/%s\" align=left vspace=10 hspace=5> </a> \n", last_Mimage);
	}

	if (post_julia == 1){
		printf("<INPUT TYPE=\"hidden\" NAME=\"sflg\" VALUE=\"%d\">\n", sflg);
		printf("<INPUT TYPE=\"hidden\" NAME=\"or\"   VALUE=\"%g\">\n", old_range);
		printf("<INPUT TYPE=\"hidden\" NAME=\"ox\"   VALUE=\"%.15g\">\n", old_x);
		printf("<INPUT TYPE=\"hidden\" NAME=\"oy\"   VALUE=\"%.15g\">\n", old_y);
		printf("<INPUT TYPE=\"hidden\" NAME=\"oms\"  VALUE=\"%d\">\n", old_map_size);
		printf("<INPUT TYPE=\"hidden\" NAME=\"last_Mimage\"  VALUE=\"%s\">\n", last_Mimage);
		printf("<INPUT TYPE=\"hidden\" NAME=\"jor\"  VALUE=\"%g\">\n", RANGE);
		printf("<INPUT TYPE=\"hidden\" NAME=\"jox\"  VALUE=\"%.15g\">\n", X);
		printf("<INPUT TYPE=\"hidden\" NAME=\"joy\"  VALUE=\"%.15g\">\n", Y);
		printf("<INPUT TYPE=\"hidden\" NAME=\"jocx\" VALUE=\"%.15g\">\n", CX);
		printf("<INPUT TYPE=\"hidden\" NAME=\"jocy\" VALUE=\"%.15g\">\n", CY);
		printf("<INPUT TYPE=\"hidden\" NAME=\"joms\" VALUE=\"%d\">\n", map_size);
		printf("<INPUT TYPE=\"hidden\" NAME=\"last_Jimage\"  VALUE=\"julia%d.gif\">\n", jnew_num);
	}
	else{
		printf("<INPUT TYPE=\"hidden\" NAME=\"sflg\" VALUE=\"1\">\n");
		printf("<INPUT TYPE=\"hidden\" NAME=\"or\"   VALUE=\"%g\">\n", RANGE);
		printf("<INPUT TYPE=\"hidden\" NAME=\"ox\"   VALUE=\"%.15g\">\n", X);
		printf("<INPUT TYPE=\"hidden\" NAME=\"oy\"   VALUE=\"%.15g\">\n", Y);
		printf("<INPUT TYPE=\"hidden\" NAME=\"oms\"  VALUE=\"%d\">\n", map_size);
		printf("<INPUT TYPE=\"hidden\" NAME=\"last_Mimage\"  VALUE=\"mandelbrot%d.gif\">\n", new_num);
		printf("<INPUT TYPE=\"hidden\" NAME=\"jor\"  VALUE=\"%g\">\n", RANGE);
		printf("<INPUT TYPE=\"hidden\" NAME=\"jox\"  VALUE=\"%.15g\">\n", jold_x);
		printf("<INPUT TYPE=\"hidden\" NAME=\"joy\"  VALUE=\"%.15g\">\n", jold_y);
		printf("<INPUT TYPE=\"hidden\" NAME=\"jocx\" VALUE=\"%.15g\">\n", jold_cx);
		printf("<INPUT TYPE=\"hidden\" NAME=\"jocy\" VALUE=\"%.15g\">\n", jold_cy);
		printf("<INPUT TYPE=\"hidden\" NAME=\"joms\" VALUE=\"%d\">\n", jold_map_size);
		printf("<INPUT TYPE=\"hidden\" NAME=\"last_Jimage\"  VALUE=\"none\">\n");
	}
	printf("<INPUT TYPE=\"hidden\" NAME=\"julia_posted\" VALUE=\"%d\">\n\n", post_julia);

	if (!strcmp(do_julia, "no")){
		printf("<font size=5>Mandelbrot Set Image #%d<br>\n", last_num+1);
		if (Y >= 0.0){
			printf("<font size=4>Central point: c<font size=1>o<font size=4> = %.15 + %.15<i>i</i><br>\n", X, Y);
		}
		else{
			printf("<font size=4>Central point: c<font size=1>o<font size=4> = %.15g - %.15g<i>i</i><br>\n", X, -Y);
		}
		printf("Range: |<i>Re</i>{c - c<font size=1>o<font size=4>}| < %.15g and |<i>Im</i>{c - c<font size=1>o<font size=4>}| < %.15g<br>\n", RANGE, RANGE);
	}
	else{
		if (sflg != 0) printf("<font size=5>Mandelbrot Set Image #%d<br>\n", last_num);
		if (old_y >= 0.0){
			printf("<font size=4>Central point: c<font size=1>o<font size=4> = %.15g + %.15g<i>i</i><br>\n", old_x, old_y);
		}
		else{
			printf("<font size=4>Central point: c<font size=1>o<font size=4> = %.15g - %.15g<i>i</i><br>\n", old_x, -old_y);
		}
		printf("Range: |<i>Re</i>{c - c<font size=1>o<font size=4>}| < %.15g and |<i>Im</i>{c - c<font size=1>o<font size=4>}| < %.15g<br>\n", old_range, old_range);
	}

	if (post_julia == 1){
		printf("<h3><a href=\"/~mgh/frac/junk/%s\" >\n", last_Mimage);
		printf("Display GIF</a></h3><br>\n");
	}
	else{
		printf("<h3><a href=\"/~mgh/frac/junk/mandelbrot%d.gif\" >\n", new_num);
		printf("Display GIF</a></h3><br>\n");
	}
	printf("<br clear=left>\n");

	if (!strcmp(do_julia, "yes")){
		printf("<INPUT TYPE=\"image\" NAME=\"Jset\" SRC=\"/~mgh/frac/junk/julia%d.gif\" align=left vspace=10 hspace=5> </a> \n", jnew_num);
		printf("<font size=5>Julia Set Image #%d<br>\n", jnew_num);
		if (CY >= 0.0){
			printf("<font size=4>C parameter: c = %g + %g<i>i</i><br>\n", CX, CY);
		}
		else{
			printf("<font size=4>C parameter: c = %g - %g<i>i</i><br>\n", CX, -CY);
		}
		if (Y >= 0.0){
			printf("<font size=4>Central point: z<font size=1>o<font size=4> = %g + %g<i>i</i><br>\n", X, Y);
		}
		else{
			printf("<font size=4>Central point: z<font size=1>o<font size=4> = %g - %g<i>i</i><br>\n", X, -Y);
		}
		printf("Range: |<i>Re</i>{z - z<font size=1>o<font size=4>}| < %g and |<i>Im</i>{z - z<font size=1>o<font size=4>}| < %g<br>\n", RANGE, RANGE);
		printf("<a href=\"/~mgh/frac/junk/julia%d.gif\" >\n", jnew_num);
		printf("Display GIF</a><br>\n");
		printf("<br clear=left>\n");
	}
	printf("<hr>\n");


	printf("<h3>\n");
	printf("Size of map: <SELECT NAME=\"Size\">\n");
	for (i=0; i<6; ++i){
		if (size_opts[i] == map_size){
			printf("                          <OPTION SELECTED> %3dx%3d\n", size_opts[i], size_opts[i]);
		}
		else{
			printf("                          <OPTION>          %3dx%3d\n", size_opts[i], size_opts[i]);
		}
	}
	printf("                      </SELECT>  \n");
	printf("Number of iterations: <SELECT NAME=\"Iterations\">\n");
	for (i=0; i<NITMAX; ++i){
		if (iter_opts[i] == max_iterations){
			printf("                          <OPTION SELECTED> %d\n", iter_opts[i]);
		}
		else{
			printf("                          <OPTION>          %d\n", iter_opts[i]);
		}
	}
	printf("                      </SELECT>  <br>\n");
	printf("Zoom factor: <SELECT NAME=\"Zoom\">\n");
	for (i=0; i<16; ++i){
		if (zoom_opts[i] == zoom){
			printf("                          <OPTION SELECTED> %.2f\n", zoom_opts[i]);
		}
		else{
			printf("                          <OPTION>          %.2f\n", zoom_opts[i]);
		}
	}
	printf("                      </SELECT>  \n");
	printf("Color table: <SELECT NAME=\"Ctab\">\n");
	for (i=0; i<nctabfiles; ++i){
		if (!strcmp(ctab_opts[i], ctab)){
			printf("                          <OPTION SELECTED> %s\n", ctab_opts[i]);
		}
		else{
			printf("                          <OPTION>          %s\n", ctab_opts[i]);
		}
	}
	printf("                      </SELECT>  <br>\n");
	printf("Generate a Julia set?: <SELECT NAME=\"Julia\">\n");
	if (post_julia == 0){
		printf("                          <OPTION>          yes\n");
		printf("                          <OPTION SELECTED> no\n");
	}
	else {
		printf("                          <OPTION SELECTED> yes\n");
		printf("                          <OPTION>          no\n");
	}
	printf("                      </SELECT>  <br>\n");


	printf("</FORM>\n\n");
	printf("<hr>\n\n");


	printf("</h3>\n\n");
	printf("<h3>\n\n");
	printf("<a href=\"/~mgh/frac/junk\" > View previously generated images</a><br>\n");
	printf("<a href=\"/~mgh/mand.shtml\" >Restart</a><br>\n");
	printf("</h3>\n\n");
	printf("<hr>\n");
	printf("<b>\n");
	printf("<font size=3> Notes: (1) If you answer yes to the \"Generate a Julia set?\"\n");
	printf("question, the c parameter is determined by clicking on the M-set map.\n");
	printf("The Julia set associated with that point will then be displayed next\n");
	printf("to the M-set map. Subsequent clicks on the M-set map results in the\n");
	printf("generation of different Julia sets while clicking on the J-set map at any\n");
	printf("time zooms into the currently defined Julia set. To continue zooming into\n");
	printf("the M-set map set the response to the \"Generate a Julia set?\" question to\n");
	printf("\"no\". (2) If you want generate more interesting Julia Set images, then\n");
	printf("first zoom into a small copy of the mandelbrot set and then generate\n");
	printf("a Julia set from a poiunt in the black regions of the M-set (3) This\n");
	printf("page uses netscape extensions so it may not look quite right on all WWW\n");
	printf("browsers! Should be using HTML-2.0 or higher.\n");
	printf("</b>\n");


	printf("\n<hr>\n");
	printf("<h2>\n");
	printf("<a href=""http://nis-www.lanl.gov/~mgh""> <img src=""mgh_icon.gif"" align=middle> Back\n");
	printf("to my homepage. </a><br>\n");
	printf("</h2>\n\n");



	printf("</body>\n");
	printf("</html>\n");
}


int jfileselect(dp)
struct dirent *dp;
{
        char temp_filename[256];
        int length;

        strcpy(temp_filename, dp->d_name);
        length = strlen(temp_filename);
        if (!strncmp(temp_filename, "julia", 5)){
                return(1);
        }
        else{
                return(0);
        }

}

int fileselect(dp)
struct dirent *dp;
{
        char temp_filename[256];
        int length;

        strcpy(temp_filename, dp->d_name);
        length = strlen(temp_filename);
        if (!strncmp(temp_filename, "mandelbrot", 10)){
                return(1);
        }
        else{
                return(0);
        }

}

int ctab_fileselect(dp)
struct dirent *dp;
{
        char tf[256];
        int length, accept;

        strcpy(tf, dp->d_name);
        length = strlen(tf);
	if (length < 5) return(0);

	accept = tf[length-4] == '.' &
	         tf[length-3] == 'd' &
	         tf[length-2] == 'a' &
	         tf[length-1] == 't';

        if (accept){
                return(1);
        }
        else{
                return(0);
        }

}


int compar(dp1, dp2)
struct dirent **dp1, **dp2;
{

        long num1, num2;

        sscanf((*dp1)->d_name, "mandelbrot%d.gif", &num1);
        sscanf((*dp2)->d_name, "mandelbrot%d.gif", &num2);


        if (num1 > num2){
                return(-1);
        }
        else if (num1 < num2){
                return(1);
        }
}

int jcompar(dp1, dp2)
struct dirent **dp1, **dp2;
{

        long num1, num2;

        sscanf((*dp1)->d_name, "julia%d.gif", &num1);
        sscanf((*dp2)->d_name, "julia%d.gif", &num2);


        if (num1 > num2){
                return(-1);
        }
        else if (num1 < num2){
                return(1);
        }
}
