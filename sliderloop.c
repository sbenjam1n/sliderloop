#include <m_pd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

static t_class *sliderloop_class;

typedef struct _event {
    double time;
    t_float value;
} t_event;

typedef struct _sliderloop {
    t_object x_obj;
    t_outlet *x_out;
    t_clock *x_clock;
    
    // Recording state
    int recording;
    double record_start;
    t_event *events;
    int event_count;
    int event_capacity;
    
    // Playback state
    int playing;
    double play_start;
    int play_index;
} t_sliderloop;

// Get current time in milliseconds
static double get_current_time(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000.0) + (tv.tv_usec / 1000.0);
}

// Add a new event to the buffer
static void add_event(t_sliderloop *x, t_float value) {
    if (x->event_count >= x->event_capacity) {
        int new_capacity = x->event_capacity * 2;
        if (new_capacity < 16) new_capacity = 16;
        t_event *new_events = realloc(x->events, new_capacity * sizeof(t_event));
        if (!new_events) return; // Memory error
        x->events = new_events;
        x->event_capacity = new_capacity;
    }
    
    double current_time = get_current_time();
    double timestamp = current_time - x->record_start;
    
    x->events[x->event_count].time = timestamp;
    x->events[x->event_count].value = value;
    x->event_count++;
}

// Schedule next playback event
static void schedule_next(t_sliderloop *x) {
    if (!x->playing || x->play_index >= x->event_count) return;
    
    double next_time = x->play_start + x->events[x->play_index].time;
    double current_time = get_current_time();
    double delay = next_time - current_time;
    
    if (delay < 0) delay = 0; // Avoid negative delays
    clock_delay(x->x_clock, delay);
}

// Clock callback for playback
static void playback_tick(t_sliderloop *x) {
    if (!x->playing || x->play_index >= x->event_count) return;
    
    // Output current value
    outlet_float(x->x_out, x->events[x->play_index].value);
    x->play_index++;
    
    // Loop or continue
    if (x->play_index >= x->event_count) {
        x->play_index = 0; // Reset to loop
        x->play_start = get_current_time(); // Reset playback start time
    }
    schedule_next(x); // Schedule next event
}

// Float input handler (slider values)
static void sliderloop_float(t_sliderloop *x, t_float f) {
    if (x->recording) {
        add_event(x, f);
    }
}

// Start recording
static void start_recording(t_sliderloop *x) {
    x->recording = 1;
    x->record_start = get_current_time();
    x->event_count = 0; // Reset event buffer
}

// Stop recording/playback
static void stop(t_sliderloop *x) {
    x->recording = 0;
    x->playing = 0;
    clock_unset(x->x_clock);
}

// Start playback
static void start_playback(t_sliderloop *x) {
    if (x->event_count == 0) return; // No data
    
    stop(x); // Stop any current actions
    x->playing = 1;
    x->play_index = 0;
    x->play_start = get_current_time();
    schedule_next(x); // Begin playback scheduling
}

static void save_to_file(t_sliderloop *x, t_symbol *filename) {
    FILE *f = fopen(filename->s_name, "w");  // Use FILE* instead of tmp_file
    if (!f) {
        pd_error(x, "Couldn't write to file: %s", filename->s_name);
        return;
    }
    
    // Write header
    fprintf(f, "sliderloop_data_v1.0\n");
    
    // Write events (time, value pairs)
    for (int i = 0; i < x->event_count; i++) {
        fprintf(f, "%.6f %.6f\n", x->events[i].time, x->events[i].value);
    }
    
    fclose(f);
    post("Saved %d automation points to %s", x->event_count, filename->s_name);
}

static void load_from_file(t_sliderloop *x, t_symbol *filename) {
    FILE *f = fopen(filename->s_name, "r");
    if (!f) {
        pd_error(x, "Couldn't read file: %s", filename->s_name);
        return;
    }
    
    // Clear existing data
    x->event_count = 0;
    
    // Read and verify header
    char header[32];
    if (!fgets(header, sizeof(header), f) || strstr(header, "sliderloop_data") != header) {
        pd_error(x, "Invalid file format");
        fclose(f);
        return;
    }
    
    // Read data
    double time, value;
    while (fscanf(f, "%lf %lf\n", &time, &value) == 2) {
        if (x->event_count >= x->event_capacity) {
            // Expand array if needed (reuse your existing resize logic)
            int new_capacity = x->event_capacity ? x->event_capacity * 2 : 64;
            t_event *new_events = realloc(x->events, new_capacity * sizeof(t_event));
            if (!new_events) {
                pd_error(x, "Memory error loading file");
                fclose(f);
                return;
            }
            x->events = new_events;
            x->event_capacity = new_capacity;
        }
        
        x->events[x->event_count].time = time;
        x->events[x->event_count].value = (t_float)value;
        x->event_count++;
    }
    
    fclose(f);
    post("Loaded %d automation points from %s", x->event_count, filename->s_name);
}
// Free allocated memory
static void sliderloop_free(t_sliderloop *x) {
    clock_free(x->x_clock);
    free(x->events);
}

// Constructor
static void *sliderloop_new(void) {
    t_sliderloop *x = (t_sliderloop *)pd_new(sliderloop_class);
    x->x_out = outlet_new(&x->x_obj, &s_float);
    x->x_clock = clock_new(x, (t_method)playback_tick);
    
    x->recording = 0;
    x->playing = 0;
    x->events = NULL;
    x->event_count = 0;
    x->event_capacity = 0;
    
    return (void *)x;
}

// Setup class
void sliderloop_setup(void) {
    sliderloop_class = class_new(gensym("sliderloop"),
        (t_newmethod)sliderloop_new,
        (t_method)sliderloop_free,
        sizeof(t_sliderloop),
        CLASS_DEFAULT,
        0);
    
    class_addfloat(sliderloop_class, sliderloop_float);

    class_addmethod(sliderloop_class, (t_method)save_to_file, gensym("save"), A_SYMBOL, 0);
    class_addmethod(sliderloop_class, (t_method)load_from_file, gensym("load"), A_SYMBOL, 0);
    class_addmethod(sliderloop_class, (t_method)start_recording, gensym("record"), 0);
    class_addmethod(sliderloop_class, (t_method)stop, gensym("stop"), 0);
    class_addmethod(sliderloop_class, (t_method)start_playback, gensym("play"), 0);
}
