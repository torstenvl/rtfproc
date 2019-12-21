// We use a return of 0 as an indication of success throughout
// the RTFRep sybsystem.  This is a shorthand for checking errors.
#define ISERROR(x)        ((x) != 0)

#define FB_Z              2048  // File buffer size
#define PB_Z              256   // Pattern buffer size


typedef char byte;        // A char is one byte in size, per the standard
typedef uint32_t uccp;    // Four bytes per Unicode codepoint


typedef struct repobj {

    FILE *is;               // Input stream
    FILE *os;               // Output stream

    byte ib[FB_Z];          // Input file buffer
    byte ibto[FB_Z];        // Input file buffer token oracle (readahead)
    byte ob[FB_Z];          // Output file buffer
    uccp pb[PB_Z];          // Pattern buffer for comparing codepoints

    size_t ibi;             // Input buffer iterator
    size_t ibtoi;           // Input buffer token oracle iterator
    size_t obi;             // Output buffer iterator
    size_t pbi;             // Pattern buffer iterator

    size_t ibz;             // Input buffer actual size
    size_t obz;             // Output buffer actual size
    size_t pbz;             // Pattern buffer actual size

    size_t pbmaps[PB_Z];    // Maps to start of rtf sequence for pb[i] in ib[]
    size_t pbmape[PB_Z];    // Maps to end of rtf sequence for pb[i] in ib[]

} repobj;


repobj *new_repobj(void);
repobj *new_repobj_from_file(const char *);
repobj *new_repobj_to_file(const char *);
repobj *new_repobj_from_file_to_file(const char *, const char *);
repobj *new_repobj_from_stream_to_stream(FILE *, FILE *);
void destroy_repobj(repobj *);
int rtf_process(repobj *);
