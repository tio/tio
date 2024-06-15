/*
 * tio - a serial device I/O tool
 *
 * Copyright (c) 2020-2022  Liam Beguin
 * Copyright (c) 2022  Martin Lund
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#define _GNU_SOURCE
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <regex.h>
#include <glib.h>
#include "configfile.h"
#include "print.h"
#include "rs485.h"
#include "misc.h"

#define CONFIG_GROUP_NAME_DEFAULT "default"

struct config_t config = {};

static void config_get_string(GKeyFile *key_file, gchar *group, gchar *key, char **dest, char *allowed_string, ...)
{
    (void)dest;
    GError *error = NULL;
    bool mismatch = true;

    gchar *string = g_key_file_get_string(key_file, group, key, &error);
    if (error != NULL)
    {
        if (error->code == G_KEY_FILE_ERROR_KEY_NOT_FOUND)
        {
            // Key not found - ignore key
            g_error_free(error);
            return;
        }
        tio_error_print("%s: %s", config.path, error->message);
        g_error_free(error);
        exit(EXIT_FAILURE);
    }

    va_list args;
    const char* current_arg = allowed_string;
    va_start(args, allowed_string);

    if (current_arg == NULL)
    {
        mismatch = false;
    }

    // Iterate through variable arguments
    while (current_arg != NULL)
    {
        if (strcmp(string, current_arg) == 0)
        {
            mismatch = false;
            break;
        }
        current_arg = va_arg(args, const char *);
    }

    if (mismatch)
    {
        tio_error_print("%s: Invalid %s value '%s' in %s profile", config.path, key, string, group);
        exit(EXIT_FAILURE);
    }

    va_end(args);

    *dest = string;
}

static void config_get_integer(GKeyFile *key_file, gchar *group, gchar *key, int *dest, int min, int max)
{
    (void)dest;
    GError *error = NULL;

    int value = g_key_file_get_integer(key_file, group, key, &error);
    if (error != NULL)
    {
        if (error->code == G_KEY_FILE_ERROR_KEY_NOT_FOUND)
        {
            // Key not found - ignore key
            g_error_free(error);
            return;
        }
        tio_error_print("%s: %s", config.path, error->message);
        g_error_free(error);
        exit(EXIT_FAILURE);
    }

    if ((value < min) || (value > max))
    {
        tio_error_print("%s: Invalid %s value '%d' in %s profile", config.path, key, value, group);
        exit(EXIT_FAILURE);
    }

    *dest = value;
}

static void config_get_bool(GKeyFile *key_file, gchar *group, gchar *key, bool *dest)
{
    (void)dest;
    GError *error = NULL;

    bool value = g_key_file_get_boolean(key_file, group, key, &error);
    if (error != NULL)
    {
        if (error->code == G_KEY_FILE_ERROR_KEY_NOT_FOUND)
        {
            // Key not found - ignore key
            g_error_free(error);
            return;
        }
        tio_error_print("%s: %s", config.path, error->message);
        g_error_free(error);
        exit(EXIT_FAILURE);
    }

    *dest = value;
}

static void config_parse_keys(GKeyFile *key_file, char *group)
{
    char *string = NULL;
    bool boolean = false;

    config_get_string(key_file, group, "device", &config.device, NULL);
    config_get_integer(key_file, group, "baudrate", &option.baudrate, 0, INT_MAX);
    config_get_integer(key_file, group, "databits", &option.databits, 5, 8);
    config_get_string(key_file, group, "flow", &string, "none", "hard", "soft", NULL);
    if (string != NULL)
    {
        option_parse_flow(string, &option.flow);
        g_free((void *)string);
        string = NULL;
    }
    config_get_integer(key_file, group, "stopbits", &option.stopbits, 1, 2);
    config_get_string(key_file, group, "parity", &string, "odd", "even", "none", "mark", "space", NULL);
    if (string != NULL)
    {
        option_parse_parity(string, &option.parity);
        g_free((void *)string);
        string = NULL;
    }

    config_get_integer(key_file, group, "output-delay", &option.output_delay, 0, INT_MAX);
    config_get_integer(key_file, group, "output-line-delay", &option.output_line_delay, 0, INT_MAX);
    config_get_string(key_file, group, "line-pulse-duration", &string, NULL);
    if (string != NULL)
    {
        option_parse_line_pulse_duration(string);
        g_free((void *)string);
        string = NULL;
    }
    config_get_string(key_file, group, "auto-connect", &string, "new", "latest", "direct", NULL);
    if (string != NULL)
    {
        option_parse_auto_connect(string, &option.auto_connect);
        g_free((void *)string);
        string = NULL;
    }
    config_get_string(key_file, group, "exclude-devices", &option.exclude_devices, NULL);
    config_get_string(key_file, group, "exclude-drivers", &option.exclude_devices, NULL);
    config_get_string(key_file, group, "exclude-tids", &option.exclude_devices, NULL);
    config_get_bool(key_file, group, "no-reconnect", &option.no_reconnect);
    config_get_bool(key_file, group, "local-echo", &option.local_echo);
    config_get_string(key_file, group, "input-mode", &string, "normal", "hex", "line", NULL);
    if (string != NULL)
    {
        option_parse_input_mode(string, &option.input_mode);
        g_free((void *)string);
        string = NULL;
    }
    config_get_string(key_file, group, "output-mode", &string, NULL);
    if (string != NULL)
    {
        option_parse_output_mode(string, &option.output_mode);
        g_free((void *)string);
        string = NULL;
    }
    config_get_bool(key_file, group, "timestamp", &boolean);
    if (boolean == true)
    {
        option.timestamp = TIMESTAMP_24HOUR;
    }
    config_get_string(key_file, group, "timestamp-format", &string, "24hour", "24hour-start", "24hour-delta", "iso8601", NULL);
    if (string != NULL)
    {
        option_parse_timestamp(string, &option.timestamp);
        g_free((void *)string);
        string = NULL;
    }
    config_get_integer(key_file, group, "timestamp-timeout", &option.timestamp_timeout, 0, INT_MAX);
    config_get_bool(key_file, group, "log", &option.log);
    config_get_string(key_file, group, "log-file", &option.log_filename, NULL);
    config_get_bool(key_file, group, "log-append", &option.log_append);
    config_get_bool(key_file, group, "log-strip", &option.log_strip);
    config_get_string(key_file, group, "map", &string, NULL);
    if (string != NULL)
    {
        option_parse_mappings(string);
        g_free((void *)string);
        string = NULL;
    }
    config_get_string(key_file, group, "color", &string, NULL);
    if (string != NULL)
    {
        if (strcmp(string, "list") == 0)
        {
            // Ignore
        }
        else if (strcmp(string, "none") == 0)
        {
            option.color = -1; // No color
        }
        else if (strcmp(string, "bold") == 0)
        {
            option.color = 256; // Bold
        }
        else
        {
            option.color = atoi(string);
            if ((option.color < 0) || (option.color > 255))
            {
                tio_error_print("%s: Invalid color value in %s profile", config.path, group);
                exit(EXIT_FAILURE);
            }
        }
        g_free((void *)string);
        string = NULL;
    }
    config_get_string(key_file, group, "socket", &option.socket, NULL);
    config_get_bool(key_file, group, "rs-485", &option.rs485);
    config_get_string(key_file, group, "rs-385-config", &string, NULL);
    if (string != NULL)
    {
        rs485_parse_config(string);
        g_free((void *)string);
        string = NULL;
    }
    config_get_string(key_file, group, "alert", &string, "bell", "blink", "none", NULL);
    if (string != NULL)
    {
        option_parse_alert(string, &option.alert);
        g_free((void *)string);
        string = NULL;
    }
    config_get_bool(key_file, group, "mute", &option.mute);
    config_get_string(key_file, group, "script", &option.script, NULL);
    config_get_string(key_file, group, "script-file", &option.script_filename, NULL);
    config_get_string(key_file, group, "script-run", &string, NULL);
    if (string != NULL)
    {
        option_parse_script_run(string, &option.script_run);
        g_free((void *)string);
        string = NULL;
    }
    config_get_string(key_file, group, "exec", &option.exec, NULL);
    config_get_string(key_file, group, "prefix-ctrl-key", &string, NULL);
    if (string != NULL)
    {
        if (strcmp(string, "none") == 0)
        {
            option.prefix_enabled = false;
        }
        else if (strlen(string) >= 2)
        {
            tio_error_print("%s: Invalid prefix-ctrl-key value in %s profile", config.path, group);
            exit(EXIT_FAILURE);
        }
        else if (ctrl_key_code(string[0]) > 0)
        {
            option.prefix_enabled = true;
            option.prefix_code = ctrl_key_code(string[0]);
            option.prefix_key = string[0];
        }
        else
        {
            tio_error_print("%s: Invalid prefix-ctrl-key value in %s profile", config.path, group);
            exit(EXIT_FAILURE);
        }
        g_free((void *)string);
        string = NULL;
    }
}

static int config_file_resolve(void)
{
    char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg)
    {
        if (asprintf(&config.path, "%s/tio/config", xdg) != -1)
        {
            if (access(config.path, F_OK) == 0)
            {
                return 0;
            }
            free(config.path);
        }
    }

    char *home = getenv("HOME");
    if (home)
    {
        if (asprintf(&config.path, "%s/.config/tio/config", home) != -1)
        {
            if (access(config.path, F_OK) == 0)
            {
                return 0;
            }
            free(config.path);
        }

        if (asprintf(&config.path, "%s/.tioconfig", home) != -1)
        {
            if (access(config.path, F_OK) == 0)
            {
                return 0;
            }
            free(config.path);
        }
    }

    config.path = NULL;
    return -EINVAL;
}

void config_file_show_profiles(void)
{
    GKeyFile *keyfile;
    GError *error = NULL;

    memset(&config, 0, sizeof(struct config_t));

    // Find config file
    if (config_file_resolve() != 0)
    {
        // None found - stop parsing
        return;
    }

    keyfile = g_key_file_new();

    if (!g_key_file_load_from_file(keyfile, config.path, G_KEY_FILE_NONE, &error))
    {
        tio_error_print("Failure loading file: %s", error->message);
        g_error_free(error);
        return;
    }

    // Get all group names
    gsize num_groups;
    gchar **group = g_key_file_get_groups(keyfile, &num_groups);

    for (gsize i = 0; i < num_groups; i++)
    {
        // Skip default group
        if (strcmp(group[i], CONFIG_GROUP_NAME_DEFAULT) == 0)
        {
            continue;
        }
        printf("%s ", group[i]);
    }

    g_strfreev(group);
    g_key_file_free(keyfile);
}

static void replace_substring(char *str, const char *substr, const char *replacement)
{
    char *pos = strstr(str, substr);
    if (pos != NULL)
    {
        int substrLen = strlen(substr);
        int replacementLen = strlen(replacement);
        memmove(pos + replacementLen, pos + substrLen, strlen(pos + substrLen) + 1);
        memcpy(pos, replacement, replacementLen);
    }
}

static char *match_and_replace(const char *str, const char *pattern, char *device)
{
    char replacement_str[PATH_MAX] = {};
    char m_key[14] = {};
    regex_t regex;

    assert(str != NULL);
    assert(pattern != NULL);
    assert(device != NULL);

    char *string = strndup(device, PATH_MAX);
    if (string == NULL)
    {
        tio_debug_printf("Failure allocating string memory\n");
        return NULL;
    }

    /* Find matches of pattern in str. For each match, replace any '%mN' in the
     * copy of the device string with the corresponding match subexpression and
     * return the new formed device string.
     *
     * Note: %m0 = Full match expression.
     *       %m1 = First subexpression
     *       %m2 = Second subexpression
     *       %m3 = etc..
     */

    if (regcomp(&regex, pattern, REG_EXTENDED) != 0)
    {
        // Failure to compile regular expression
        tio_error_print("Failure compiling regular expression '%s'\n", pattern);
        exit(EXIT_FAILURE);
    }

    regmatch_t matches[regex.re_nsub + 1];
    int status = regexec(&regex, str, regex.re_nsub + 1, matches, 0);
    if (status == 0)
    {
        tio_debug_printf("Full match: ");
        int j = 0;
        for (int i = matches[0].rm_so; i < matches[0].rm_eo; i++)
        {
            tio_debug_printf_raw("%c", str[i]);
            replacement_str[j++] = str[i];
        }
        replacement_str[j] = '\0';
        replace_substring(string, "%m0", replacement_str);
        tio_debug_printf_raw("\n");

        for (int i = 1; i < ((int)regex.re_nsub + 1) && matches[i].rm_so != -1; i++)
        {
            tio_debug_printf("Subexpression %d match: ", i);
            int k = 0;
            for (int l = matches[i].rm_so; l < matches[i].rm_eo; l++)
            {
                tio_debug_printf_raw("%c", str[l]);
                replacement_str[k++] = str[l];
            }
            replacement_str[k] = '\0';
            sprintf(m_key, "%%m%d", i);
            replace_substring(string, m_key, replacement_str);
            tio_debug_printf_raw("\n");
        }
    }
    else if (status == REG_NOMATCH)
    {
        tio_debug_printf("No regex match\n");
        goto error;
    }
    else
    {
        char error_message[100];
        regerror(status, &regex, error_message, sizeof(error_message));
        tio_debug_printf("Regex match failed: %s", error_message);
        goto error;
    }

    regfree(&regex);
    return string;

error:
    regfree(&regex);
    return NULL;
}

void config_file_parse(void)
{
    // Find config file
    if (config_file_resolve() != 0)
    {
        // None found - stop parsing
        return;
    }

    if (option.target == NULL)
    {
        return;
    }

    GKeyFile *keyfile = g_key_file_new();
    GError *error = NULL;

    if (g_key_file_load_from_file(keyfile, config.path, G_KEY_FILE_NONE, &error) == false)
    {
        tio_error_print("Failure loading file %s: %s", config.path, error->message);
        g_error_free(error);
        exit(EXIT_FAILURE);
    }

    // Parse default group/section
    if (g_key_file_has_group(keyfile, CONFIG_GROUP_NAME_DEFAULT))
    {
        config_parse_keys(keyfile, CONFIG_GROUP_NAME_DEFAULT);
    }

    // Parse target
    if (g_key_file_has_group(keyfile, option.target))
    {
        config.active_group = strdup(option.target);
        config_parse_keys(keyfile, option.target);
    }
    else
    {
        // Find group by pattern
        gsize num_groups;
        gchar **group = g_key_file_get_groups(keyfile, &num_groups);

        for (gsize i = 0; i < num_groups; i++)
        {
            // Skip default group
            if (strcmp(group[i], CONFIG_GROUP_NAME_DEFAULT) == 0)
            {
                continue;
            }

            // Lookup 'pattern' key
            gchar *pattern = g_key_file_get_string(keyfile, group[i], "pattern", &error);
            if (error != NULL)
            {
                g_error_free(error);
                error = NULL;
                continue;
            }

            // Lookup 'device' key
            gchar *device = g_key_file_get_string(keyfile, group[i], "device", &error);
            if (error != NULL)
            {
                g_error_free(error);
                error = NULL;
                continue;
            }

            // Match pattern against target and replace any sub expression
            // matches (%mN) in device string and return resulting string
            // representing the new pattern based string.
            config.device = match_and_replace(option.target, pattern, device);
            if (config.device != NULL)
            {
                // Match found - save device
                device = strdup(config.device);

                // Parse found group (may replace config.device)
                config_parse_keys(keyfile, group[i]);

                // Update configuration
                config.active_group = strdup(group[i]);
                config.device = device; // Restore new device string

                break;
            }
        }

        g_strfreev(group);
    }

    g_key_file_free(keyfile);

    atexit(&config_exit);
}

void config_exit(void)
{
    free(config.active_group);
    free(config.path);
    free(config.device);
}

void config_file_print(void)
{
    if (config.path != NULL)
    {
        tio_printf(" Active configuration file: %s", config.path);
        if (config.active_group != NULL)
        {
            tio_printf(" Active configuration profile: %s", config.active_group);
        }
    }
}

void config_list_targets(void)
{
    memset(&config, 0, sizeof(struct config_t));

    // Find config file
    if (config_file_resolve() != 0)
    {
        // None found
        return;
    }

    GKeyFile *keyfile;
    GError *error = NULL;

    keyfile = g_key_file_new();

    if (!g_key_file_load_from_file(keyfile, config.path, G_KEY_FILE_NONE, &error))
    {
        g_error_free(error);
        return;
    }

    // Get all group names
    gsize num_groups;
    gchar **group = g_key_file_get_groups(keyfile, &num_groups);

    if (num_groups == 0)
    {
        return;
    }

    printf("\nConfiguration profiles (%s)\n", config.path);
    printf("--------------------------------------------------------------------------------\n");

    int j = 1;
    for (gsize i = 0; i < num_groups; i++)
    {
        // Skip default group
        if (strcmp(group[i], CONFIG_GROUP_NAME_DEFAULT) == 0)
        {
            continue;
        }
        printf("%-19s ", group[i]);
        if (j++ % 4 == 0)
        {
            putchar('\n');
        }
    }
    if ((j-1) % 4 != 0)
    {
        putchar('\n');
    }

    g_strfreev(group);
    g_key_file_free(keyfile);
}
