/*
    tg
    Copyright (C) 2015 Marcello Mamino

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 2 as
    published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "tg.h"

#ifdef DEBUG
int testing = 0;
#endif

int preset_bph[] = PRESET_BPH;

cairo_pattern_t *black,*white,*red,*green,*blue,*blueish,*yellow;

void print_debug(char *format,...)
{
	va_list args;
	va_start(args,format);
	vfprintf(stderr,format,args);
	va_end(args);
}

void error(char *format,...)
{
	char s[100];
	va_list args;

	va_start(args,format);
	int size = vsnprintf(s,100,format,args);
	va_end(args);

	char *t;
	if(size < 100) {
		t = s;
	} else {
		t = alloca(size+1);
		va_start(args,format);
		vsnprintf(t,size+1,format,args);
		va_end(args);
	}

	fprintf(stderr,"%s\n",t);

#ifdef DEBUG
	if(testing) return;
#endif

	GtkWidget *dialog = gtk_message_dialog_new(NULL,0,GTK_MESSAGE_ERROR,GTK_BUTTONS_CLOSE,"%s",t);
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
}

void define_color(cairo_pattern_t **gc,double r,double g,double b)
{
	*gc = cairo_pattern_create_rgb(r,g,b);
}

void initialize_palette()
{
	define_color(&black,0,0,0);
	define_color(&white,1,1,1);
	define_color(&red,1,0,0);
	define_color(&green,0,0.8,0);
	define_color(&blue,0,0,1);
	define_color(&blueish,0,0,.5);
	define_color(&yellow,1,1,0);
}

void redraw(struct main_window *w)
{
	gtk_widget_queue_draw(w->output_drawing_area);
	gtk_widget_queue_draw(w->tic_drawing_area);
	gtk_widget_queue_draw(w->toc_drawing_area);
	gtk_widget_queue_draw(w->period_drawing_area);
	gtk_widget_queue_draw(w->paperstrip_drawing_area);
#ifdef DEBUG
	gtk_widget_queue_draw(w->debug_drawing_area);
#endif
}

/* Find the preset bph value closest to the current period */
int guess_bph(double period)
{
	double bph = 7200 / period;
	double min = bph;
	int i,ret;

	ret = 0;
	for(i=0; preset_bph[i]; i++) {
		double diff = fabs(bph - preset_bph[i]);
		if(diff < min) {
			min = diff;
			ret = i;
		}
	}

	return preset_bph[ret];
}

struct processing_buffers *get_data(struct main_window *w, int *old)
{
	*old = w->is_old;
	if(w->old)
		w->guessed_bph = w->bph ? w->bph : guess_bph(w->old->period / w->sample_rate);
	return w->old;
}

void draw_graph(double a, double b, cairo_t *c, struct processing_buffers *p, GtkWidget *da)
{
	GtkAllocation temp;
	gtk_widget_get_allocation (da, &temp);
	int width = temp.width;
	int height = temp.height;

	int n;

	int first = 1;
	for(n=0; n<2*width; n++) {
		int i = n < width ? n : 2*width - 1 - n;
		double x = fmod(a + i * (b-a) / width, p->period);
		if(x < 0) x += p->period;
		int j = floor(x);
		double y;

		if(p->waveform[j] <= 0) y = 0;
		else y = p->waveform[j] * 0.4 / p->waveform_max;

		int k = round(y*height);
		if(n < width) k = -k;

		if(first) {
			cairo_move_to(c,i+.5,height/2+k+.5);
			first = 0;
		} else
			cairo_line_to(c,i+.5,height/2+k+.5);
	}
}

#ifdef DEBUG
void draw_debug_graph(double a, double b, cairo_t *c, struct processing_buffers *p, GtkWidget *da)
{
	int width = da->allocation.width;
	int height = da->allocation.height;

	int i;
	float max = 0;

	int ai = round(a);
	int bi = 1+round(b);
	if(ai < 0) ai = 0;
	if(bi > p->sample_count) bi = p->sample_count;
	for(i=ai; i<bi; i++)
		if(p->debug[i] > max)
			max = p->debug[i];

	int first = 1;
	for(i=0; i<width; i++) {
		if( round(a + i*(b-a)/width) != round(a + (i+1)*(b-a)/width) ) {
			int j = round(a + i*(b-a)/width);
			if(j < 0) j = 0;
			if(j >= p->sample_count) j = p->sample_count-1;

			int k = round((0.1+p->debug[j]/max)*0.8*height);

			if(first) {
				cairo_move_to(c,i+.5,height-k-.5);
				first = 0;
			} else
				cairo_line_to(c,i+.5,height-k-.5);
		}
	}
}
#endif

double amplitude_to_time(double lift_angle, double amp)
{
	return asin(lift_angle / (2 * amp)) / M_PI;
}

double draw_watch_icon(cairo_t *c, int signal, int happy)
{
	happy = !!happy;
	cairo_set_line_width(c,3);
	cairo_set_source(c,happy?green:red);
	cairo_move_to(c, OUTPUT_WINDOW_HEIGHT * 0.5, OUTPUT_WINDOW_HEIGHT * 0.5);
	cairo_line_to(c, OUTPUT_WINDOW_HEIGHT * 0.75, OUTPUT_WINDOW_HEIGHT * (0.75 - 0.5*happy));
	cairo_move_to(c, OUTPUT_WINDOW_HEIGHT * 0.5, OUTPUT_WINDOW_HEIGHT * 0.5);
	cairo_line_to(c, OUTPUT_WINDOW_HEIGHT * 0.35, OUTPUT_WINDOW_HEIGHT * (0.65 - 0.3*happy));
	cairo_stroke(c);
	cairo_arc(c, OUTPUT_WINDOW_HEIGHT * 0.5, OUTPUT_WINDOW_HEIGHT * 0.5, OUTPUT_WINDOW_HEIGHT * 0.4, 0, 2*M_PI);
	cairo_stroke(c);
	const int l = OUTPUT_WINDOW_HEIGHT * 0.8 / (2*NSTEPS - 1);
	int i;
	cairo_set_line_width(c,1);
	for(i = 0; i < signal; i++) {
		cairo_move_to(c, OUTPUT_WINDOW_HEIGHT + 0.5*l, OUTPUT_WINDOW_HEIGHT * 0.9 - 2*i*l);
		cairo_line_to(c, OUTPUT_WINDOW_HEIGHT + 1.5*l, OUTPUT_WINDOW_HEIGHT * 0.9 - 2*i*l);
		cairo_line_to(c, OUTPUT_WINDOW_HEIGHT + 1.5*l, OUTPUT_WINDOW_HEIGHT * 0.9 - (2*i+1)*l);
		cairo_line_to(c, OUTPUT_WINDOW_HEIGHT + 0.5*l, OUTPUT_WINDOW_HEIGHT * 0.9 - (2*i+1)*l);
		cairo_line_to(c, OUTPUT_WINDOW_HEIGHT + 0.5*l, OUTPUT_WINDOW_HEIGHT * 0.9 - 2*i*l);
		cairo_stroke_preserve(c);
		cairo_fill(c);
	}
	return OUTPUT_WINDOW_HEIGHT + 3*l;
}

double get_rate(int bph, double sample_rate, struct processing_buffers *p)
{
	return (7200/(bph*p->period / sample_rate) - 1)*24*3600;
}

cairo_t *cairo_init(GtkWidget *widget)
{
	cairo_t *c = gdk_cairo_create(gtk_widget_get_window(widget));
	cairo_set_line_width(c,1);

	cairo_set_source(c,black);
	cairo_paint(c);

	return c;
}

double print_s(cairo_t *c, double x, double y, char *s)
{
	cairo_text_extents_t extents;
	cairo_move_to(c,x,y);
	cairo_show_text(c,s);
	cairo_text_extents(c,s,&extents);
	x += extents.x_advance;
	return x;
}

double print_number(cairo_t *c, double x, double y, char *s)
{
	cairo_text_extents_t extents;
	cairo_text_extents(c,"0",&extents);
	double z = extents.x_advance;
	char t[2];
	t[1] = 0;
	while((t[0] = *s++)) {
		cairo_text_extents(c,t,&extents);
		cairo_move_to(c, x + (z - extents.x_advance) / 2, y);
		cairo_show_text(c,t);
		x += z;
	}
	return x;
}

gboolean output_expose_event(GtkWidget *widget, GdkEvent *event, struct main_window *w)
{
	cairo_t *c = cairo_init(widget);

	int old;
	struct processing_buffers *p = get_data(w,&old);

	double x = draw_watch_icon(c,w->signal,w->calibrating ? w->signal==NSTEPS : w->signal);

	cairo_text_extents_t extents;

	cairo_set_font_size(c, OUTPUT_FONT);
	cairo_text_extents(c,"0",&extents);
	double y = (double)OUTPUT_WINDOW_HEIGHT/2 - extents.y_bearing - extents.height/2;

	if(w->calibrating) {
		cairo_set_source(c, white);
		x = print_s(c,x,y,"cal");
		cairo_set_font_size(c, OUTPUT_FONT*2/3);
		x = print_s(c,x,y," (");
		cairo_move_to(c,x,y);
		{
			double a = 0;
			char *s[] = {"wait", "acq.", "done", "fail", NULL}, **t = s;
			for(;*t;t++) {
				cairo_text_extents(c,*t,&extents);
				if(a < extents.x_advance) a = extents.x_advance;
			}
			x += a;
		}
		switch(w->cdata->state) {
			case 1:
				cairo_set_source(c,green);
				cairo_show_text(c,"done");
				break;
			case 0:
				cairo_set_source(c, w->signal == NSTEPS ? white : yellow);
				cairo_show_text(c, w->signal == NSTEPS ? "acq." : "wait");
				break;
			case -1:
				cairo_set_source(c,red);
				cairo_show_text(c,"fail");
				break;
		}
		cairo_set_source(c, white);
		x = print_s(c,x,y,")");
		cairo_set_font_size(c, OUTPUT_FONT);
		char s[20];
		double cal = w->cdata->calibration;
		switch(w->cdata->state) {
			case 1:
				sprintf(s," %s%.1f",cal<0?"-":"+",fabs(cal));
				x = print_s(c,x,y,s);
				cairo_set_font_size(c, OUTPUT_FONT*2/3);
				x = print_s(c,x,y," s/d");
				break;
			case 0:
				sprintf(s," %d",100*w->cdata->wp/w->cdata->size);
				x = print_number(c,x,y,s);
				x = print_s(c,x,y," %");
				break;
		}
	} else {
		char outputs[8][20];
		if(p) {
			int bph = w->guessed_bph;
			int rate = round(get_rate(bph, w->sample_rate, p));
			double be = fabs(p->be) * 1000 / p->sample_rate;
			char rates[20];
			sprintf(rates,"%s%d",rate > 0 ? "+" : rate < 0 ? "-" : "",abs(rate));
			sprintf(outputs[0],"%4s",rates);
			sprintf(outputs[2]," %4.1f",be);
			if(p->amp > 0)
				sprintf(outputs[4]," %3.0f",p->amp);
			else
				strcpy(outputs[4]," ---");
		} else {
			strcpy(outputs[0],"----");
			strcpy(outputs[2]," ----");
			strcpy(outputs[4]," ---");
		}
		sprintf(outputs[6]," %d",w->guessed_bph);

		strcpy(outputs[1]," s/d");
		strcpy(outputs[3]," ms");
		strcpy(outputs[5]," deg");
		strcpy(outputs[7]," bph");

		int i;
		for(i=0; i<8; i++) {
			if(i%2) {
				cairo_set_source(c, white);
				cairo_set_font_size(c, OUTPUT_FONT*2/3);
				x = print_s(c,x,y,outputs[i]);
			} else {
				cairo_set_source(c, i > 4 || !p || !old ? white : yellow);
				cairo_set_font_size(c, OUTPUT_FONT);
				x = print_number(c,x,y,outputs[i]);
			}
		}
	}
#ifdef DEBUG
	{
		static GTimer *timer = NULL;
		if (!timer) timer = g_timer_new();
		else {
			char s[100];
			sprintf(s,"  %.2f fps",1./g_timer_elapsed(timer, NULL));
			cairo_set_source(c, white);
			cairo_set_font_size(c, OUTPUT_FONT);
			x = print_s(c,x,y,s);
			g_timer_reset(timer);
		}
	}
#endif

	cairo_destroy(c);

	return FALSE;
}

void expose_waveform(
		struct main_window *w,
		GtkWidget *da,
		int (*get_offset)(struct processing_buffers*),
		double (*get_pulse)(struct processing_buffers*))
{
	cairo_t *c = cairo_init(da);

	GtkAllocation temp;
	gtk_widget_get_allocation (da, &temp);

	int width = temp.width;
	int height = temp.height;

	gtk_widget_get_allocation (w->window, &temp);
	int font = temp.width / 90;
	if(font < 12)
		font = 12;
	int i;

	cairo_set_font_size(c,font);

	for(i = 1-NEGATIVE_SPAN; i < POSITIVE_SPAN; i++) {
		int x = (NEGATIVE_SPAN + i) * width / (POSITIVE_SPAN + NEGATIVE_SPAN);
		cairo_move_to(c, x + .5, height / 2 + .5);
		cairo_line_to(c, x + .5, height - .5);
		if(i%5)
			cairo_set_source(c,green);
		else
			cairo_set_source(c,red);
		cairo_stroke(c);
	}
	cairo_set_source(c,white);
	for(i = 1-NEGATIVE_SPAN; i < POSITIVE_SPAN; i++) {
		if(!(i%5)) {
			int x = (NEGATIVE_SPAN + i) * width / (POSITIVE_SPAN + NEGATIVE_SPAN);
			char s[10];
			sprintf(s,"%d",i);
			cairo_move_to(c,x+font/4,height-font/2);
			cairo_show_text(c,s);
		}
	}

	cairo_text_extents_t extents;

	cairo_text_extents(c,"ms",&extents);
	cairo_move_to(c,width - extents.x_advance - font/4,height-font/2);
	cairo_show_text(c,"ms");

	int old;
	struct processing_buffers *p = get_data(w,&old);
	double period = p ? p->period / w->sample_rate : 7200. / w->guessed_bph;

	for(i = 10; i < 360; i+=10) {
		if(2*i < w->la) continue;
		double t = period*amplitude_to_time(w->la,i);
		if(t > .001 * NEGATIVE_SPAN) continue;
		int x = round(width * (NEGATIVE_SPAN - 1000*t) / (NEGATIVE_SPAN + POSITIVE_SPAN));
		cairo_move_to(c, x+.5, .5);
		cairo_line_to(c, x+.5, height / 2 + .5);
		if(i % 50)
			cairo_set_source(c,green);
		else
			cairo_set_source(c,red);
		cairo_stroke(c);
	}

	double last_x = 0;
	cairo_set_source(c,white);
	for(i = 50; i < 360; i+=50) {
		double t = period*amplitude_to_time(w->la,i);
		if(t > .001 * NEGATIVE_SPAN) continue;
		int x = round(width * (NEGATIVE_SPAN - 1000*t) / (NEGATIVE_SPAN + POSITIVE_SPAN));
		if(x > last_x) {
			char s[10];

			sprintf(s,"%d",abs(i));
			cairo_move_to(c, x + font/4, font * 3 / 2);
			cairo_show_text(c,s);
			cairo_text_extents(c,s,&extents);
			last_x = x + font/4 + extents.x_advance;
		}
	}

	cairo_text_extents(c,"deg",&extents);
	cairo_move_to(c,width - extents.x_advance - font/4,font * 3 / 2);
	cairo_show_text(c,"deg");

	if(p) {
		double span = 0.001 * w->sample_rate;
		int offset = get_offset(p);

		double a = offset - span * NEGATIVE_SPAN;
		double b = offset + span * POSITIVE_SPAN;

		draw_graph(a,b,c,p,da);

		cairo_set_source(c,old?yellow:white);
		cairo_stroke_preserve(c);
		cairo_fill(c);

		double pulse = get_pulse(p);
		if(pulse > 0) {
			int x = round((NEGATIVE_SPAN - pulse * 1000 / p->sample_rate) * width / (POSITIVE_SPAN + NEGATIVE_SPAN));
			cairo_move_to(c, x, 1);
			cairo_line_to(c, x, height - 1);
			cairo_set_source(c,blue);
			cairo_set_line_width(c,2);
			cairo_stroke(c);
		}
	} else {
		cairo_move_to(c, .5, height / 2 + .5);
		cairo_line_to(c, width - .5, height / 2 + .5);
		cairo_set_source(c,yellow);
		cairo_stroke(c);
	}

	cairo_destroy(c);
}

int get_tic(struct processing_buffers *p)
{
	return p->tic;
}

int get_toc(struct processing_buffers *p)
{
	return p->toc;
}

double get_tic_pulse(struct processing_buffers *p)
{
	return p->tic_pulse;
}

double get_toc_pulse(struct processing_buffers *p)
{
	return p->toc_pulse;
}

gboolean tic_expose_event(GtkWidget *widget, GdkEvent *event, struct main_window *w)
{
	expose_waveform(w, w->tic_drawing_area, get_tic, get_tic_pulse);
	return FALSE;
}

gboolean toc_expose_event(GtkWidget *widget, GdkEvent *event, struct main_window *w)
{
	expose_waveform(w, w->toc_drawing_area, get_toc, get_toc_pulse);
	return FALSE;
}

gboolean period_expose_event(GtkWidget *widget, GdkEvent *event, struct main_window *w)
{
	cairo_t *c = cairo_init(widget);

	GtkAllocation temp;
	gtk_widget_get_allocation (w->period_drawing_area, &temp);

	int width = temp.width;
	int height = temp.height;

	int old;
	struct processing_buffers *p = get_data(w,&old);

	double toc,a=0,b=0;

	if(p) {
		toc = p->tic < p->toc ? p->toc : p->toc + p->period;
		a = ((double)p->tic + toc)/2 - p->period/2;
		b = ((double)p->tic + toc)/2 + p->period/2;

		cairo_move_to(c, (p->tic - a - NEGATIVE_SPAN*.001*w->sample_rate) * width/p->period, 0);
		cairo_line_to(c, (p->tic - a - NEGATIVE_SPAN*.001*w->sample_rate) * width/p->period, height);
		cairo_line_to(c, (p->tic - a + POSITIVE_SPAN*.001*w->sample_rate) * width/p->period, height);
		cairo_line_to(c, (p->tic - a + POSITIVE_SPAN*.001*w->sample_rate) * width/p->period, 0);
		cairo_set_source(c,blueish);
		cairo_fill(c);

		cairo_move_to(c, (toc - a - NEGATIVE_SPAN*.001*w->sample_rate) * width/p->period, 0);
		cairo_line_to(c, (toc - a - NEGATIVE_SPAN*.001*w->sample_rate) * width/p->period, height);
		cairo_line_to(c, (toc - a + POSITIVE_SPAN*.001*w->sample_rate) * width/p->period, height);
		cairo_line_to(c, (toc - a + POSITIVE_SPAN*.001*w->sample_rate) * width/p->period, 0);
		cairo_set_source(c,blueish);
		cairo_fill(c);
	}

	int i;
	for(i = 1; i < 16; i++) {
		int x = i * width / 16;
		cairo_move_to(c, x+.5, .5);
		cairo_line_to(c, x+.5, height - .5);
		if(i % 4)
			cairo_set_source(c,green);
		else
			cairo_set_source(c,red);
		cairo_stroke(c);
	}

	if(p) {
		draw_graph(a,b,c,p,w->period_drawing_area);

		cairo_set_source(c,old?yellow:white);
		cairo_stroke_preserve(c);
		cairo_fill(c);
	} else {
		cairo_move_to(c, .5, height / 2 + .5);
		cairo_line_to(c, width - .5, height / 2 + .5);
		cairo_set_source(c,yellow);
		cairo_stroke(c);
	}

	cairo_destroy(c);

	return FALSE;
}

gboolean paperstrip_expose_event(GtkWidget *widget, GdkEvent *event, struct main_window *w)
{
	int i,old;
	uint64_t time = get_timestamp();
	struct processing_buffers *p = get_data(w,&old);
	double sweep;
	int zoom_factor;
	double slope = 1000; // detected rate: 1000 -> do not display
	if(w->calibrating) {
		sweep = w->nominal_sr;
		zoom_factor = PAPERSTRIP_ZOOM_CAL;
		struct calibration_data *d = w->cdata;
		for(i=d->wp-1; i >= 0 &&
			d->events[i] / w->nominal_sr > w->events[w->events_wp] / w->nominal_sr;
			i--);
		for(i++; i<d->wp; i++) {
			if(d->events[i] / w->nominal_sr <= w->events[w->events_wp] / w->nominal_sr)
				continue;
			if(++w->events_wp == EVENTS_COUNT) w->events_wp = 0;
			w->events[w->events_wp] = d->events[i];
			debug("event at %llu\n",w->events[w->events_wp]);
		}
		w->events_from = time;
		slope = (double) w->cal * zoom_factor / (10 * 3600 * 24);
	} else {
		sweep = w->sample_rate * 3600. / w->guessed_bph;
		zoom_factor = PAPERSTRIP_ZOOM;
		if(p && !old) {
			uint64_t last = w->events[w->events_wp];
			for(i=0; i<EVENTS_MAX && p->events[i]; i++)
				if(p->events[i] > last + floor(p->period / 4)) {
					if(++w->events_wp == EVENTS_COUNT) w->events_wp = 0;
					w->events[w->events_wp] = p->events[i];
					debug("event at %llu\n",w->events[w->events_wp]);
				}
			w->events_from = p->timestamp - ceil(p->period);
		} else {
			w->events_from = time;
		}
		if(p && w->events[w->events_wp]) {
			double rate = get_rate(w->guessed_bph, w->sample_rate, p);
			slope = - rate * zoom_factor / (3600. * 24.);
		}
	}

	cairo_t *c = cairo_init(widget);

	GtkAllocation temp;
	gtk_widget_get_allocation (w->paperstrip_drawing_area, &temp);

	int width = temp.width;
	int height = temp.height;

	int stopped = 0;
	if(w->events[w->events_wp] && time > 5 * w->sample_rate + w->events[w->events_wp]) {
		time = 5 * w->sample_rate + w->events[w->events_wp];
		stopped = 1;
	}

	int strip_width = round(width / (1 + PAPERSTRIP_MARGIN));

	cairo_set_line_width(c,1.3);

	slope *= strip_width;
	if(slope <= 2 && slope >= -2) {
		for(i=0; i<4; i++) {
			double y = 0;
			cairo_move_to(c, (double)width * (i+.5) / 4, 0);
			for(;;) {
				double x = y * slope + (double)width * (i+.5) / 4;
				x = fmod(x, width);
				if(x < 0) x += width;
				double nx = x + slope * (height - y);
				if(nx >= 0 && nx <= width) {
					cairo_line_to(c, nx, height);
					break;
				} else {
					double d = slope > 0 ? width - x : x;
					y += d / fabs(slope);
					cairo_line_to(c, slope > 0 ? width : 0, y);
					y += 1;
					if(y > height) break;
					cairo_move_to(c, slope > 0 ? 0 : width, y);
				}
			}
		}
		cairo_set_source(c, blue);
		cairo_stroke(c);
	}

	cairo_set_line_width(c,1);

	int left_margin = (width - strip_width) / 2;
	int right_margin = (width + strip_width) / 2;
	cairo_move_to(c, left_margin + .5, .5);
	cairo_line_to(c, left_margin + .5, height - .5);
	cairo_move_to(c, right_margin + .5, .5);
	cairo_line_to(c, right_margin + .5, height - .5);
	cairo_set_source(c, green);
	cairo_stroke(c);

	double now = sweep*ceil(time/sweep);
	double ten_s = w->sample_rate * 10 / sweep;
	double last_line = fmod(now/sweep, ten_s);
	int last_tenth = floor(now/(sweep*ten_s));
	for(i=0;;i++) {
		double y = 0.5 + round(last_line + i*ten_s);
		if(y > height) break;
		cairo_move_to(c, .5, y);
		cairo_line_to(c, width-.5, y);
		cairo_set_source(c, (last_tenth-i)%6 ? green : red);
		cairo_stroke(c);
	}

	cairo_set_source(c,stopped?yellow:white);
	for(i = w->events_wp;;) {
		if(!w->events[i]) break;
		double event = now - w->events[i] + w->trace_centering + sweep * PAPERSTRIP_MARGIN / (2 * zoom_factor);
		int column = floor(fmod(event, (sweep / zoom_factor)) * strip_width / (sweep / zoom_factor));
		int row = floor(event / sweep);
		if(row >= height) break;
		cairo_move_to(c,column,row);
		cairo_line_to(c,column+1,row);
		cairo_line_to(c,column+1,row+1);
		cairo_line_to(c,column,row+1);
		cairo_line_to(c,column,row);
		cairo_fill(c);
		if(column < width - strip_width && row > 0) {
			column += strip_width;
			row -= 1;
			cairo_move_to(c,column,row);
			cairo_line_to(c,column+1,row);
			cairo_line_to(c,column+1,row+1);
			cairo_line_to(c,column,row+1);
			cairo_line_to(c,column,row);
			cairo_fill(c);
		}
		if(--i < 0) i = EVENTS_COUNT - 1;
		if(i == w->events_wp) break;
	}

	cairo_set_source(c,white);
	cairo_set_line_width(c,2);
	cairo_move_to(c, left_margin + 3, height - 20.5);
	cairo_line_to(c, right_margin - 3, height - 20.5);
	cairo_stroke(c);
	cairo_set_line_width(c,1);
	cairo_move_to(c, left_margin + .5, height - 20.5);
	cairo_line_to(c, left_margin + 5.5, height - 15.5);
	cairo_line_to(c, left_margin + 5.5, height - 25.5);
	cairo_line_to(c, left_margin + .5, height - 20.5);
	cairo_fill(c);
	cairo_move_to(c, right_margin + .5, height - 20.5);
	cairo_line_to(c, right_margin - 4.5, height - 15.5);
	cairo_line_to(c, right_margin - 4.5, height - 25.5);
	cairo_line_to(c, right_margin + .5, height - 20.5);
	cairo_fill(c);

	char s[100];
	cairo_text_extents_t extents;

	gtk_widget_get_allocation (w->window, &temp);
	int font = temp.width / 90;
	if(font < 12)
		font = 12;
	cairo_set_font_size(c,font);

	sprintf(s, "%.1f ms", w->calibrating ?
				1000. / zoom_factor :
				3600000. / (w->guessed_bph * zoom_factor));
	cairo_text_extents(c,s,&extents);
	cairo_move_to(c, (width - extents.x_advance)/2, height - 30);
	cairo_show_text(c,s);

	cairo_destroy(c);

	return FALSE;
}

#ifdef DEBUG
gboolean debug_expose_event(GtkWidget *widget, GdkEvent *event, struct main_window *w)
{
	cairo_t *c = cairo_init(widget);

	int old = 0;
	struct processing_buffers *p = w->calibrating ?
		&w->pdata->buffers[0] : get_data(w,&old);

	if(p) {
		//double a = p->period / 10;
		//double b = p->period * 2;
		double a = w->sample_rate / 10;
		double b = w->sample_rate * 2;

		draw_debug_graph(a,b,c,p,w->debug_drawing_area);

		cairo_set_source(c,old?yellow:white);
		cairo_stroke(c);
	}

	cairo_destroy(c);

	return FALSE;
}
#endif

guint kick_computer(struct main_window *);
void handle_bph_change(GtkComboBox *b, struct main_window *w)
{
	char *s = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(b));
	if(s) {
		int n;
		char *t;
		n = (int)strtol(s,&t,10);
		if(*t || n < MIN_BPH || n > MAX_BPH) w->bph = 0;
		else w->bph = w->guessed_bph = n;
		g_free(s);
	}
	kick_computer(w);
}

void handle_la_change(GtkSpinButton *b, struct main_window *w)
{
	double la = gtk_spin_button_get_value(b);
	if(la < MIN_LA || la > MAX_LA) la = DEFAULT_LA;
	w->la = la;
	redraw(w);
}

void update_sample_rate(struct main_window *w)
{
	w->sample_rate = w->nominal_sr * (1 + (double) w->cal / (10 * 3600 * 24));
}

void handle_cal_change(GtkSpinButton *b, struct main_window *w)
{
	w->cal = gtk_spin_button_get_value(b);;
	update_sample_rate(w);
	redraw(w);
}

gboolean output_cal(GtkSpinButton *spin, gpointer data)
{
	GtkAdjustment *adj;
	gchar *text;
	int value;

	adj = gtk_spin_button_get_adjustment (spin);
	value = (int)gtk_adjustment_get_value (adj);
	text = g_strdup_printf ("%c%d.%d", value < 0 ? '-' : '+', abs(value)/10, abs(value)%10);
	gtk_entry_set_text (GTK_ENTRY (spin), text);
	g_free (text);

	return TRUE;
}

gboolean input_cal(GtkSpinButton *spin, double *val, gpointer data)
{
	double x = 0;
	sscanf(gtk_entry_get_text (GTK_ENTRY (spin)), "%lf", &x);
	int n = round(x*10);
	if(n < MIN_CAL) n = MIN_CAL;
	if(n > MAX_CAL) n = MAX_CAL;
	*val = n;

	return TRUE;
}

void handle_clear_trace(GtkButton *b, struct main_window *w)
{
	memset(w->events,0,EVENTS_COUNT*sizeof(uint64_t));
	redraw(w);
}

void handle_center_trace(GtkButton *b, struct main_window *w)
{
	uint64_t last_ev = w->events[w->events_wp];
	if(last_ev) {
		double sweep;
		if(w->calibrating)
			sweep = (double) w->sample_rate / PAPERSTRIP_ZOOM_CAL;
		else
			sweep = w->sample_rate * 3600. / (PAPERSTRIP_ZOOM * w->guessed_bph);
		w->trace_centering = fmod(last_ev + .5*sweep , sweep);
	} else
		w->trace_centering = 0;
	redraw(w);
}

void handle_calibrate(GtkToggleButton *b, struct main_window *w)
{
	int button_state = gtk_toggle_button_get_active(b) == TRUE;
	if(button_state != w->calibrate) {
		w->calibrate = button_state;
		pthread_mutex_lock(&w->recompute_mutex);
		if(w->recompute >= 0) w->recompute = 1;
		pthread_cond_signal(&w->recompute_cond);
		pthread_mutex_unlock(&w->recompute_mutex);
		g_source_remove(w->computer_kicker);
		w->computer_kicker = g_timeout_add_full(G_PRIORITY_LOW,
				w->calibrate?1000:100,(GSourceFunc)kick_computer,w,NULL);
	}
}

gboolean quit(struct main_window *w)
{
	pthread_mutex_lock(&w->recompute_mutex);
	w->recompute = -1;
	pthread_cond_signal(&w->recompute_cond);
	pthread_mutex_unlock(&w->recompute_mutex);
	return FALSE;
}

gboolean delete_event(GtkWidget *widget, GdkEvent *event, gpointer w)
{
	debug("Received delete event\n");
	quit((struct main_window *)w);
	return TRUE;
}

/* Set up the main window and populate with widgets */
void init_main_window(struct main_window *w)
{
	w->signal = 0;

	w->events = malloc(EVENTS_COUNT * sizeof(uint64_t));
	memset(w->events,0,EVENTS_COUNT * sizeof(uint64_t));
	w->events_wp = 0;
	w->events_from = 0;
	w->trace_centering = 0;

	w->guessed_bph = w->last_bph = w->bph ? w->bph : DEFAULT_BPH;

	w->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	gtk_container_set_border_width(GTK_CONTAINER(w->window), 10); // Border around the window
	g_signal_connect(w->window, "delete_event", G_CALLBACK(delete_event), w);

	gtk_window_set_title(GTK_WINDOW(w->window), PROGRAM_NAME " " VERSION);

	GtkWidget *vbox = gtk_vbox_new(FALSE, 10); // Replaced by GtkGrid in GTK+ 3.2
	gtk_container_add(GTK_CONTAINER(w->window), vbox);
	gtk_widget_show(vbox);

	GtkWidget *hbox = gtk_hbox_new(FALSE, 10); // Replaced by GtkGrid in GTK+ 3.2
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);
	gtk_widget_show(hbox);

	// BPH label
	GtkWidget *label = gtk_label_new("bph");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	// BPH combo box
	GtkWidget *bph_combo_box = gtk_combo_box_text_new_with_entry();
	gtk_box_pack_start(GTK_BOX(hbox), bph_combo_box, FALSE, TRUE, 0);
	// Fill in pre-defined values
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(bph_combo_box), "guess");
	int i,current = 0;
	for(i = 0; preset_bph[i]; i++) {
		char s[100];
		sprintf(s,"%d", preset_bph[i]);
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(bph_combo_box), s);
		if(w->bph == preset_bph[i]) current = i+1;
	}
	if(current || w->bph == 0)
		gtk_combo_box_set_active(GTK_COMBO_BOX(bph_combo_box), current);
	else {
		char s[32];
		sprintf(s,"%d",w->bph);
		GtkEntry *e = GTK_ENTRY(gtk_bin_get_child(GTK_BIN(bph_combo_box)));
		gtk_entry_set_text(e,s);
	}
	g_signal_connect (bph_combo_box, "changed", G_CALLBACK(handle_bph_change), w);
	gtk_widget_show(bph_combo_box);

	// Lift angle label
	label = gtk_label_new("lift angle");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	// Lift angle spin button
	GtkWidget *la_spin_button = gtk_spin_button_new_with_range(MIN_LA, MAX_LA, 1);
	gtk_box_pack_start(GTK_BOX(hbox), la_spin_button, FALSE, TRUE, 0);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(la_spin_button), w->la);
	g_signal_connect(la_spin_button, "value_changed", G_CALLBACK(handle_la_change), w);
	gtk_widget_show(la_spin_button);

	// Lift angle label
	label = gtk_label_new("cal");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	// Calibration spin button
	w->cal_spin_button = gtk_spin_button_new_with_range(MIN_CAL, MAX_CAL, 1);
	gtk_box_pack_start(GTK_BOX(hbox), w->cal_spin_button, FALSE, TRUE, 0);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w->cal_spin_button), w->cal);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(w->cal_spin_button), FALSE);
	gtk_entry_set_width_chars(GTK_ENTRY(w->cal_spin_button), 6);
	g_signal_connect(w->cal_spin_button, "value_changed", G_CALLBACK(handle_cal_change), w);
	g_signal_connect(w->cal_spin_button, "output", G_CALLBACK(output_cal), NULL);
	g_signal_connect(w->cal_spin_button, "input", G_CALLBACK(input_cal), NULL);
	gtk_widget_show(w->cal_spin_button);

	// CALIBRATE button
	GtkWidget *cal_button = gtk_toggle_button_new_with_label("Calibrate");
	gtk_box_pack_end(GTK_BOX(hbox), cal_button, FALSE, FALSE, 0);
	g_signal_connect(cal_button, "toggled", G_CALLBACK(handle_calibrate), w);
	gtk_widget_show(cal_button);

	// Info area on top
	w->output_drawing_area = gtk_drawing_area_new();
	gtk_widget_set_size_request(w->output_drawing_area, 700, OUTPUT_WINDOW_HEIGHT);
	gtk_box_pack_start(GTK_BOX(vbox),w->output_drawing_area, FALSE, TRUE, 0);
	g_signal_connect (w->output_drawing_area, "expose_event", G_CALLBACK(output_expose_event), w);
	gtk_widget_set_events(w->output_drawing_area, GDK_EXPOSURE_MASK);
	gtk_widget_show(w->output_drawing_area);

	GtkWidget *hbox2 = gtk_hbox_new(FALSE, 10); // Replaced by GtkGrid in GTK+ 3.2
	gtk_box_pack_start(GTK_BOX(vbox), hbox2, TRUE, TRUE, 0);
	gtk_widget_show(hbox2);

	GtkWidget *vbox2 = gtk_vbox_new(FALSE, 10); // Replaced by GtkGrid in GTK+ 3.2
	gtk_box_pack_start(GTK_BOX(hbox2), vbox2, FALSE, TRUE, 0);
	gtk_widget_show(vbox2);

	// Paperstrip
	w->paperstrip_drawing_area = gtk_drawing_area_new();
	gtk_widget_set_size_request(w->paperstrip_drawing_area, 200, 400);
	gtk_box_pack_start(GTK_BOX(vbox2), w->paperstrip_drawing_area, TRUE, TRUE, 0);
	g_signal_connect (w->paperstrip_drawing_area, "expose_event", G_CALLBACK(paperstrip_expose_event), w);
	gtk_widget_set_events(w->paperstrip_drawing_area, GDK_EXPOSURE_MASK);
	gtk_widget_show(w->paperstrip_drawing_area);

	GtkWidget *hbox3 = gtk_hbox_new(FALSE, 10); // Replaced by GtkGrid in GTK+ 3.2
	gtk_box_pack_start(GTK_BOX(vbox2), hbox3, FALSE, TRUE, 0);
	gtk_widget_show(hbox3);

	// CLEAR button
	GtkWidget *clear_button = gtk_button_new_with_label("Clear");
	gtk_box_pack_start(GTK_BOX(hbox3), clear_button, TRUE, TRUE, 0);
	g_signal_connect (clear_button, "clicked", G_CALLBACK(handle_clear_trace), w);
	gtk_widget_show(clear_button);

	// CENTER button
	GtkWidget *center_button = gtk_button_new_with_label("Center");
	gtk_box_pack_start(GTK_BOX(hbox3), center_button, TRUE, TRUE, 0);
	g_signal_connect (center_button, "clicked", G_CALLBACK(handle_center_trace), w);
	gtk_widget_show(center_button);

	GtkWidget *vbox3 = gtk_vbox_new(FALSE,10); // Replaced by GtkGrid in GTK+ 3.2
	gtk_box_pack_start(GTK_BOX(hbox2), vbox3, TRUE, TRUE, 0);
	gtk_widget_show(vbox3);

	// Tic waveform area
	w->tic_drawing_area = gtk_drawing_area_new();
	gtk_widget_set_size_request(w->tic_drawing_area, 700, 100);
	gtk_box_pack_start(GTK_BOX(vbox3), w->tic_drawing_area, TRUE, TRUE, 0);
	g_signal_connect (w->tic_drawing_area, "expose_event", G_CALLBACK(tic_expose_event), w);
	gtk_widget_set_events(w->tic_drawing_area, GDK_EXPOSURE_MASK);
	gtk_widget_show(w->tic_drawing_area);

	// Toc waveform area
	w->toc_drawing_area = gtk_drawing_area_new();
	gtk_widget_set_size_request(w->toc_drawing_area, 700, 100);
	gtk_box_pack_start(GTK_BOX(vbox3), w->toc_drawing_area, TRUE, TRUE, 0);
	g_signal_connect (w->toc_drawing_area, "expose_event", G_CALLBACK(toc_expose_event), w);
	gtk_widget_set_events(w->toc_drawing_area, GDK_EXPOSURE_MASK);
	gtk_widget_show(w->toc_drawing_area);

	// Period waveform area
	w->period_drawing_area = gtk_drawing_area_new();
	gtk_widget_set_size_request(w->period_drawing_area, 700, 100);
	gtk_box_pack_start(GTK_BOX(vbox3), w->period_drawing_area, TRUE, TRUE, 0);
	g_signal_connect (w->period_drawing_area, "expose_event", G_CALLBACK(period_expose_event), w);
	gtk_widget_set_events(w->period_drawing_area, GDK_EXPOSURE_MASK);
	gtk_widget_show(w->period_drawing_area);

#ifdef DEBUG
	w->debug_drawing_area = gtk_drawing_area_new();
	gtk_widget_set_size_request(w->debug_drawing_area, 500, 100);
	gtk_box_pack_start(GTK_BOX(vbox3), w->debug_drawing_area, TRUE, TRUE, 0);
	g_signal_connect (w->debug_drawing_area, "expose_event", G_CALLBACK(debug_expose_event), w);
	gtk_widget_set_events(w->debug_drawing_area, GDK_EXPOSURE_MASK);
	gtk_widget_show(w->debug_drawing_area);
#endif

	gtk_window_maximize(GTK_WINDOW(w->window));
	gtk_widget_show(w->window);
	gtk_window_set_focus(GTK_WINDOW(w->window), NULL);
}

guint refresh(struct main_window *w)
{
	redraw(w);
	return FALSE;
}

guint close_main_window(struct main_window *w)
{
	debug("Closing main window\n");
	gtk_widget_destroy(w->window);
	gtk_main_quit();
	return FALSE;
}

void *computing_thread(void *void_w)
{
	struct main_window *w = void_w;
	for(;;) {
		pthread_mutex_lock(&w->recompute_mutex);
		while(!w->recompute)
			pthread_cond_wait(&w->recompute_cond, &w->recompute_mutex);
		if(w->recompute > 0) w->recompute = 0;
		pthread_mutex_unlock(&w->recompute_mutex);

		if(w->recompute < 0) {
			debug("Terminating computation thread\n");
			gdk_threads_add_idle((GSourceFunc)close_main_window,w);
			return NULL;
		}

		gdk_threads_enter();
		int calibrate = w->calibrate;
		int bph = w->bph;
		double la = w->la;
		uint64_t events_from = w->events_from;
		gdk_threads_leave();

		if(calibrate && !w->calibrating) {
			w->cdata->wp = 0;
			w->cdata->state = 0;
			w->cal_updated = 0;
		}

		int signal = calibrate ?
			analyze_pa_data_cal(w->pdata, w->cdata) :
			analyze_pa_data(w->pdata, bph, la, events_from);

		gdk_threads_enter();

		if(calibrate != w->calibrating)
			memset(w->events,0,EVENTS_COUNT*sizeof(uint64_t));

		w->calibrating = calibrate;

		if(calibrate) {
			w->signal = signal;
			if(w->old) {
				pb_destroy_clone(w->old);
				w->old = NULL;
				w->is_old = signal < NSTEPS;
			}
			if(w->cdata->state == 1 && !w->cal_updated) {
				w->cal_updated = 1;
				w->cal = round(10 * w->cdata->calibration);
				gtk_spin_button_set_value(GTK_SPIN_BUTTON(w->cal_spin_button), w->cal);
			}
		} else {
			struct processing_buffers *p = w->pdata->buffers;
			int i;
			for(i=0; i<NSTEPS && p[i].ready; i++);
			for(i--; i>=0 && p[i].sigma > p[i].period / 10000; i--);
			if(i>=0) {
				if(w->old) pb_destroy_clone(w->old);
				w->old = pb_clone(&p[i]);
				w->is_old = 0;
				w->signal = i == NSTEPS-1 && p[i].amp < 0 ? signal-1 : signal;
			} else {
				w->is_old = 1;
				w->signal = -signal;
			}
		}

		gdk_threads_leave();

		gdk_threads_add_idle((GSourceFunc)refresh,w);
	}
}

guint kick_computer(struct main_window *w)
{
	pthread_mutex_lock(&w->recompute_mutex);
	if(w->recompute >= 0) w->recompute = 1;
	pthread_cond_signal(&w->recompute_cond);
	pthread_mutex_unlock(&w->recompute_mutex);

	return TRUE;
}

guint save_on_change_timer(struct main_window *w)
{
	save_on_change(w);
	return TRUE;
}

int run_interface()
{
	int nominal_sr;
	double real_sr;

	if(start_portaudio(&nominal_sr, &real_sr)) return 1;

	struct processing_buffers p[NSTEPS];
	int i;
	for(i=0; i<NSTEPS; i++) {
		p[i].sample_rate = nominal_sr;
		p[i].sample_count = nominal_sr * (1<<(i+FIRST_STEP));
		setup_buffers(&p[i]);
		p[i].period = -1;
	}

	struct processing_data pd;
	pd.buffers = p;
	pd.last_tic = 0;

	struct calibration_data cd;
	setup_cal_data(&cd);

	struct main_window w;
	w.nominal_sr = nominal_sr;
	w.cal = MIN_CAL - 1;
	w.bph = 0;
	w.la = DEFAULT_LA;
	w.pdata = &pd;
	w.cdata = &cd;
	w.old = NULL;
	w.is_old = 1;
	w.recompute = 0;
	w.calibrate = 0;
	w.calibrating = 0;

	load_config(&w);

	if(w.la < MIN_LA || w.la > MAX_LA) w.la = DEFAULT_LA;
	if(w.bph < MIN_BPH || w.bph > MAX_BPH) w.bph = 0;
	if(w.cal < MIN_CAL || w.cal > MAX_CAL)
		w.cal = (real_sr - nominal_sr) * (3600*24) / nominal_sr;
	update_sample_rate(&w);

	init_main_window(&w);

	if(    pthread_mutex_init(&w.recompute_mutex, NULL)
	    || pthread_cond_init(&w.recompute_cond, NULL)
	    || pthread_create(&w.computing_thread, NULL, computing_thread, &w)) {
		error("Unable to initialize computational thread");
		return 1;
	}

	w.computer_kicker = g_timeout_add_full(G_PRIORITY_LOW,100,(GSourceFunc)kick_computer,&w,NULL);
	g_timeout_add_full(G_PRIORITY_LOW,10000,(GSourceFunc)save_on_change_timer,&w,NULL);
#ifdef DEBUG
	if(testing)
		g_timeout_add_full(G_PRIORITY_LOW,3000,(GSourceFunc)quit,&w,NULL);
#endif

	gtk_main();
	debug("Main loop has terminated\n");

	save_config(&w);

	// We leak the processing buffers, program is terminating anyway
	return terminate_portaudio();
}

int main(int argc, char **argv)
{
#if !GLIB_CHECK_VERSION(2,31,0)
	g_thread_init(NULL);
#endif
	gdk_threads_init();
	gdk_threads_enter();

	gtk_init(&argc, &argv);
#ifdef DEBUG
	if(argc > 1 && !strcmp("test",argv[1]))
		testing = 1;
#endif
	initialize_palette();

	int ret = run_interface();
	debug("Interface exited with status %d\n",ret);

	gdk_threads_leave();

	return ret;
}
