#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <oauth.h>
#include <curl/curl.h>
#include <jansson.h>

#define TWITTER_STREAM "https://stream.twitter.com/1.1/statuses/sample.json"
#define DEFAULT_CONFIG "config.json" /* If no config file is passed, this name is used by default */
#define DEFAULT_TIMEOUT 5000l /* Default length the curl requests goes on until it times out */

struct oauth_info {
    const char *consumer_key;
    const char *consumer_secret;
    const char *access_token;
    const char *access_token_secret;
};

struct file_info {
    const char *directory;
    const char *prefix;
    long timeout;
};

struct oauth_file_info {
    struct oauth_info *oauth_details;
    struct file_info *file_details;
};

struct oauth_info* load_ouath(char *configfile);
struct file_info* load_settings(char *configfile);
char* output_file(struct file_info *file_details);

int main(int argc, char *argv[]) {
    FILE *out, *test;
    CURL *curl;
    struct oauth_info *oauth_details;
    struct file_info *file_details;
    char *signedurl;
    int curlstatus;

    if(argc > 2) {
        printf("ERROR: Incorrect usage. Please pass the config JSON file name\n"\
               "       or if no file is specified \"config.json\" will be used by default.");
    }
    else {
        if((test = fopen(argv[1], "r"))) {
            fclose(test);
            oauth_details = load_ouath(argv[1]);
            file_details = load_settings(argv[1]);
        }
        else if((test = fopen(DEFAULT_CONFIG, "r"))) {
            fclose(test);
            oauth_details = load_ouath(DEFAULT_CONFIG);
            file_details = load_settings(DEFAULT_CONFIG);
        }
        else {
            printf("No valid config file found.\nPlease create a config file named config.json or"\
                   "pass the name as an argument to the program.");
            exit(EXIT_FAILURE);
        }
    }

    /* Debugging printf() calls to see what we are parsing */
    /*
    printf("%s\n%s\n%s\n%s\n%s\n%s\n", oauth[0], oauth[1], oauth[2], oauth[3], file_details[0], file_details[1]);
    printf("%s\n", output_file(file_details));
    */

    out = fopen(output_file(file_details), "w+");

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();

    /* URL, POST parameters (not used in this example), OAuth signing method, HTTP method, keys */
    signedurl = oauth_sign_url2(TWITTER_STREAM, NULL, OA_HMAC, "GET",
                                (*oauth_details).consumer_key,
                                (*oauth_details).consumer_secret,
                                (*oauth_details).access_token,
                                (*oauth_details).access_token_secret);

    /* URL we're connecting to */
    curl_easy_setopt(curl, CURLOPT_URL, signedurl);

    /* User agent we're going to use, fill this in appropriately */
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "appname/0.1");

    /* libcurl will now fail on an HTTP error (>=400) */
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);

    /* Set how long we want to collect tweets before we stop. (in milliseconds) */
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, (*file_details).timeout);

    /* In this case, we're not specifying a callback function for
       handling received data, so libcURL will use the default, which
       is to write to the file specified in WRITEDATA */
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) out);

    /* Execute the request! */
    curlstatus = curl_easy_perform(curl);
    printf("curl_easy_perform terminated with status code %d\n", curlstatus);

    curl_easy_cleanup(curl);
    curl_global_cleanup();
    fclose(out);

    return EXIT_SUCCESS;
}

/*
 * Function: load_ouath
 * Parameter(s):    char *configfile   Pointer to a string that contains the config file's name
 *
 * Returns: A pointer to a struct containing the oauth details.
 * Description: Loads the OAUTH info from a config file and returns the necessary information to
 *              make valid requests to the Twitter API in a pointer to a struct.
 */
struct oauth_info* load_ouath(char *configfile) {
    json_error_t error;
    FILE *config = fopen(configfile, "r");
    struct oauth_info *oauth_details = (struct oauth_info*) calloc(1, sizeof(struct oauth_info));
    json_t *root = json_loadf(config, 0, &error);
    json_t *oauth = json_object_get(root, "oauth");
    json_t *app = json_object_get(oauth, "app");
    json_t *account = json_object_get(oauth, "account");

    fclose(config);

    (*oauth_details).consumer_key = json_string_value(json_object_get(app, "consumer_key"));
    (*oauth_details).consumer_secret = json_string_value(json_object_get(app, "consumer_secret"));
    (*oauth_details).access_token = json_string_value(json_object_get(account, "access_token"));
    (*oauth_details).access_token_secret = json_string_value(json_object_get(account, "access_token_secret"));

    return oauth_details;
}

/*
 * Function: load_settings
 * Parameter(s):    char *configfile   Pointer to a string that contains the config file's name
 *
 * Returns: A pointer to a structure containing the settings for naming the output file.
 * Description: Loads the settings necessary to properly collect and store scraped tweets
 *              in a struct from a config file.
 */
struct file_info* load_settings(char *configfile) {
    json_error_t error;
    FILE *config = fopen(configfile, "r");
    struct file_info *file_details = (struct file_info *) calloc(1, sizeof(struct file_info));
    json_t *root = json_loadf(config, 0, &error);
    json_t *settings = json_object_get(root, "settings");

    fclose(config);

    (*file_details).directory = json_string_value(json_object_get(settings, "directory"));
    (*file_details).prefix = json_string_value(json_object_get(settings, "prefix"));
    (*file_details).timeout = json_integer_value(json_object_get(settings, "timeout"));
    (*file_details).directory = (*file_details).directory != NULL ? (*file_details).directory : "";
    (*file_details).prefix = (*file_details).prefix != NULL ? (*file_details).prefix : "";
    (*file_details).timeout = (*file_details).timeout > 0 ? (*file_details).timeout : DEFAULT_TIMEOUT;

    return file_details;
}

/*
 * Function: output_file
 * Parameter(s):    struct file_info *file_details   Pointer to a structure containing the details
 *                                                   from which to name the output file.
 *
 * Returns: An pointer to a string that contains the output file's name after the directory, prefix,
 *          and time details have been concantenated.
 *          File names adhere to the following scheme:
 *          "DIRECTORY/PREFIX[MONTH_DAY_YEAR]@[HH:MM:SS].json"
 * Description: Construct the name for the output file and return a pointer to a string containing
 *              the name.
 */
char* output_file(struct file_info *file_details) {
    char *out_filename, *mkdir_cmd, *curr_time_str = (char *) malloc(40 * sizeof(char));
    time_t curr_time = time(NULL);
    struct tm *time_details = localtime(&curr_time);

    strftime(curr_time_str, 40, "[%b_%d_%Y]@[%H:%M:%S].json", time_details);

    mkdir_cmd = (char *) malloc((10 + strlen((*file_details).directory)) * sizeof(char));
    sprintf(mkdir_cmd, "mkdir -p %s", (*file_details).directory);
    system(mkdir_cmd);

    out_filename = (char *) calloc(40 + strlen((*file_details).directory) + strlen((*file_details).prefix), sizeof(char));
    sprintf(out_filename, "%s/%s%s", (*file_details).directory, (*file_details).prefix, curr_time_str);

    return out_filename;
}
