/*
 * edsm.h
 */

#ifndef GEOEXP_H_
#define GEOEXP_H_

#include "edsm.h"
#include "messages.h"
#include <boost/filesystem.hpp>
#include "lstar.h"
#include "blueStar.h"
#include "utilities.h"
#include "geodb.h"
#include <unordered_map>

#include <boost/accumulators/accumulators.hpp>	// For Accumulators
#include <boost/accumulators/statistics.hpp>			// To calculate mean and variance for an accumulator

#include <math.h>

#include "bluefringe.h"

#define MAX_LENGTH_SAMPLES_POS 	1000
#define MAX_LENGTH_SAMPLES_NEG 	1000


using namespace std;


/*! \class geoExp
    \brief Class for EDSM inference algorithm.

    Folder hierarchy: the root folder is "experiments". Into it there are all the experiments, everyone with its folder.
    Into every folder of particular experiment there are the folders for each prefix length of analyzed user.
 */
//namespace gi
//{
class geoExp
{
private:

	string 		db_path;

	string 		exec_path;														/*!< The project folder, parent of execution directory */
	string		root_exp_path;												/*!< Dir with all experiments: "experiments" */
	string		current_exp_folder;											/*!< Folder for current experiment, charartterizated by user */

	int 			user;
	int 			min_prefixes_length;										/*!< Length of geohash strings */
	int 			max_prefixes_length;										/*!< Length of geohash strings */

	int 			cross_val_run = -1;											/*!< Number of run for cross-validation */

	bool 		edsm;

	bool		blueStar;
	double 	alpha_value;													/*!< Alpha value for blue star */
	double 	delta_value;													/*!< Parameter for Z-value. Final evaluations should be indipendent from that particular value \
	 	 	 	 	 	 	 	 	 	 	 	 	 	 	 	 	 	 	 	 	 	 	 	 	for numeric issues is better if it is greater than 1000 */

	int			training_proportion;										/*!< Training proportion of strings in percentage */
	int			test_proportion;												/*!< Test proportion of strings in percentage */
	double	cold_start_proportion;										/*!< Proportion of strings for a "cold start" user */

	bool		no_repetitions_inside_strings;						/*!< If true delete the repetitions of symbols inside strings; substituted with one occurence*/

	string 		geo_alph =														/*!< Geohash alphabet symbols */
					  { '0', '1', '2', '3', '4', '5', '6', '7',
						'8', '9', 'b', 'c', 'd', 'e', 'f', 'g',
						'h', 'j', 'k', 'm', 'n', 'p', 'q', 'r',
						's', 't', 'u', 'v', 'w', 'x', 'y', 'z' };


	// Users similarity
	static const int n_compared_users = 10;

	const int int_comp_user[n_compared_users] = {4, 17, 25, 41, 62, 85, 128, 140, 144, 153};

	map<string, bool> comp_user;									/*!< Map with users id in string-format; assosiated bool is used into inference algorithm */


	// Cold start
	static const int n_compared_users_coldstart = 3;

	const int int_comp_user_coldstart[n_compared_users_coldstart] = {8, 17, 62};

	string comp_user_coldstart[n_compared_users_coldstart];		/*!< Map with users id in string-format; assosiated bool is used into inference algorithm */


	// Structure for statistics
	struct mystat {
		string 		prefix = "";																// Identify the inference process
		int 			num_states_edsm 	  						= -1;
		int*			num_states_bluestar;
		double 	percentage_positive_edsm				= -1;				// Cicla le LUNGHEZZE dei prefissi (for statistical purpose... see below)
		double* 	percentage_positive_bluestar;
		double* 	errore_rate_bluestar;												// Error rate on the training dataset
		int			num_actual_merges_edsm 			= -1;
		int			num_heuristic_evaluations_edsm 	= -1;
		int*			num_actual_merges_bluestar;
		int*			num_heuristic_evaluations_bluestar ;
	};


	void 		set_root_exp_folders();

	void 		set_current_exp_folder();

	string 		create_folder(const string  base_path, const string new_folder, bool current_time);

	int 			write_minitraj_from_db_like_samples_TRAINTEST_TESTSET(string user,string prefix, string path_samples);

	void 		write_minitrajectories_as_training_set(vector<string>* p_samples, vector<string>* n_samples, const char * file_path);

	void 		write_minitrajectories_as_test_set(vector<string>* test_samples, const char * file_path);

	string* 	read_testsamples_leaving_geohash_encoding(const char * /*path*/ path_samples, int &dim_positive, int* &wp);

	double 	make_generalization_test(gi::dfa* finaldfa, const char * file_path/*, int dim_test*/);

	string		add_space_between_characters_delete_repetitions(string old_string);



//  SIMILARITA'

	void 		write_minitraj_from_db_like_samples_PAIR_USERS(string user1, string user2, string prefix, string path_samples);

	void 		write_minitraj_from_db_like_samples_ALL_TRAININGSET(string user,string prefix, string path_samples);

	void 		write_similarity_matrix(map< string, map<string, gi::dfa*>> users_dfa, string current_prefix, string path_samples);

	void 		write_similarity_accuracy_on_traininget(map< string, map<string, gi::dfa*>> users_dfa, string current_prefix, string path_samples);



// COLD START

	void 		write_minitraj_from_db_like_samples_0_25_TRAINTEST_TESTSET(string user,string prefix, string path_samples);

	void 		write_similarity_accuracy_on_testset_COLDSTART(string user, map<string, gi::dfa*> users_dfa, string current_prefix, string path_samples);


	void		print_execution_parent_folder();

	void 		print_experiments_folder();


	void 		write_statistics_files(mystat* userstat, int n_prefixes, string utente, bool is_edsm, int curr_run_cross_val);

	void 		stat_write_generalization_rate(mystat* userstat, int n_prefixes, string utente, bool is_edsm, int curr_run_cross_val);

	void 		stat_write_num_states(mystat* userstat, int n_prefixes, string utente, bool is_edsm, int curr_run_cross_val);

	void 		stat_write_error_rate_on_trainigset(mystat* userstat, int n_prefixes, string utente, bool is_edsm, int curr_run_cross_val);

	void 		stat_heuristic_evaluations(mystat* userstat, int n_prefixes, string utente, bool is_edsm, int curr_run_cross_val);

	void 		stat_actual_merges(mystat* userstat, int n_prefixes, string utente, bool is_edsm, int curr_run_cross_val);

	void 		stat_final_results(mystat* userstat, int n_prefixes, string utente, bool is_edsm);

	void 		stat_final_results_minimal(mystat* userstat, int n_prefixes, string utente, bool is_edsm);


	void 		get_num_trajectories_for_pref_length(int prefixes_length);


public:

	/**
	 * Instance an object with all the members and methods for EDSM inference process.
	 * @param path It's the path where find positive and negative samples
	 */
	geoExp(string db_path, int user, int min_prefixes_length, int max_prefixes_length, bool repetitions, int train_proportion, double cold_start_proportion,\
					bool alg_edsm, bool alg_blues, double alpha, double delta);


	/**
	 * Destroy and EDSM object, freeing the memory.
	 */
	~geoExp();


	void 		run_inference_accuracy();


	void 		run_inference_similarity();


	void 		run_inference_coldstart_similarity();

};

//}


#endif /* EDSM_H_ */
