#ifndef MC_CLIENT_SOUND_SOUND_FILE_H
#define MC_CLIENT_SOUND_SOUND_FILE_H

#include "mc/client/sound/sound.h"

typedef struct mc_sound_file mc_sound_file;
typedef struct mc_sound_stream mc_sound_stream;

#define MC_SOUND_FILE_MAX_PATH_BYTES 1048576u
#define MC_SOUND_STREAM_MAX_BYTES 268435456u

/* Resolve an already absolute, strict UTF8 native filename. This is the native
 * File seam used by imported audio, not a general Java File constructor or a
 * working-directory resolver. Separators and trailing separators follow the
 * selected JDK17 File profile; dot segments remain. The actual directory query
 * supplies the trailing URL slash. A missing file still has a valid identity.
 * URL.path uses URI path quoting, not form encoding, and retains ordinary
 * Unicode as UTF16. The resolver does not open or decode the file.
 * Outputs remain unchanged on failure. Calls require exclusive access. */
mc_sound_status mc_sound_file_resolve(const char *native_utf8_path, mc_sound_file **out,
                                      mc_sound_error *error);
bool mc_sound_file_read_identity(const mc_sound_file *file, mc_sound_file_identity *out);
void mc_sound_file_free(mc_sound_file *file);

/* Open one actual regular binary file. The file owner may be freed after this
 * call: the stream owns its handle and initial MUS state. mus selects the
 * original np transform, seeded from the last segment of the resolved URL.path,
 * including percent escapes. It never guesses a seed from a playable header.
 * max_bytes is a native input bound in 1..MC_SOUND_STREAM_MAX_BYTES, independent
 * of the original SoundSystem configuration. All outputs stay unchanged on
 * admission/open failure. No graphics, audio device or JVM is initialized. */
mc_sound_status mc_sound_stream_open(const mc_sound_file *file, bool mus, size_t max_bytes,
                                     mc_sound_stream **out, mc_sound_error *error);
/* Open the retained identity of a selected SoundPool entry directly. The
 * identity is borrowed only for this call. Its original URL.path supplies the
 * seed; playback must not recompute it from a later directory query. */
mc_sound_status mc_sound_stream_open_identity(const mc_sound_file_identity *identity, bool mus,
                                              size_t max_bytes, mc_sound_stream **out,
                                              mc_sound_error *error);

/* A successful read publishes the actual prefix; zero count means EOF. A
 * partial underlying I/O failure is reported on the next call, after that
 * prefix has advanced the MUS state. Zero capacity performs no I/O. Invalid
 * arguments leave count unchanged. The destination must not overlap this owner.
 * MUS single-byte reads retain Java's signed-byte return quirk: a decoded FF
 * returns -1 with eof=false. Native EOF is separately observable as eof=true. */
mc_sound_status mc_sound_stream_read(mc_sound_stream *stream, uint8_t *destination, size_t capacity,
                                     size_t *count, mc_sound_error *error);
mc_sound_status mc_sound_stream_read_byte(mc_sound_stream *stream, int32_t *value, bool *eof,
                                          mc_sound_error *error);

/* Source close is intentionally distinct from native retirement. np inherits
 * InputStream.close(), which does nothing; closing a MUS facade leaves its
 * underlying stream readable. Raw file close closes once. free always releases
 * the actual native handle, including after EOF/failure/MUS source close.
 * A close failure still consumes the native handle. free(NULL) succeeds. */
mc_sound_status mc_sound_stream_close(mc_sound_stream *stream, mc_sound_error *error);
mc_sound_status mc_sound_stream_free(mc_sound_stream *stream, mc_sound_error *error);

#endif
