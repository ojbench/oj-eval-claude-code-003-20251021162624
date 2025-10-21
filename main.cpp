#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <sstream>
#include <unordered_map>
#include <functional>
#include <memory>

using namespace std;

enum class Status {
    Accepted,
    Wrong_Answer,
    Runtime_Error,
    Time_Limit_Exceed
};

struct Submission {
    string problem_name;
    string team_name;
    Status status;
    int time;
};

struct TeamProblem {
    bool solved = false;
    int first_solve_time = -1;
    int wrong_attempts = 0;
    int frozen_wrong_attempts = 0;
    bool is_frozen = false;

    int get_total_wrong_attempts() const {
        return wrong_attempts + frozen_wrong_attempts;
    }

    int get_penalty() const {
        if (!solved) return 0;
        return 20 * wrong_attempts + first_solve_time;
    }
};

struct Team {
    string name;
    int solved_count = 0;
    int total_penalty = 0;
    vector<int> solve_times;
    unordered_map<string, TeamProblem> problems;
    int rank = 0;

    void update_stats(const vector<string>& problem_names, bool include_frozen = false) {
        solved_count = 0;
        total_penalty = 0;
        solve_times.clear();

        for (const auto& problem_name : problem_names) {
            const auto& problem = problems[problem_name];
            if (problem.solved && (include_frozen || !problem.is_frozen)) {
                solved_count++;
                total_penalty += problem.get_penalty();
                solve_times.push_back(problem.first_solve_time);
            }
        }

        sort(solve_times.begin(), solve_times.end(), greater<int>());
    }

    bool operator<(const Team& other) const {
        if (solved_count != other.solved_count) {
            return solved_count > other.solved_count;
        }
        if (total_penalty != other.total_penalty) {
            return total_penalty < other.total_penalty;
        }
        for (size_t i = 0; i < min(solve_times.size(), other.solve_times.size()); i++) {
            if (solve_times[i] != other.solve_times[i]) {
                return solve_times[i] < other.solve_times[i];
            }
        }
        return name < other.name;
    }
};

class ICPCSystem {
private:
    bool competition_started = false;
    bool frozen = false;
    int duration_time = 0;
    int problem_count = 0;
    vector<string> problem_names;
    unordered_map<string, Team> teams;
    vector<Submission> submissions;
    vector<Team*> ranking;
    bool ranking_dirty = true;

public:
    void add_team(const string& team_name) {
        if (competition_started) {
            cout << "[Error]Add failed: competition has started.\n";
            return;
        }

        if (teams.find(team_name) != teams.end()) {
            cout << "[Error]Add failed: duplicated team name.\n";
            return;
        }

        teams[team_name] = Team();
        teams[team_name].name = team_name;
        cout << "[Info]Add successfully.\n";
    }

    void start_competition(int duration, int count) {
        if (competition_started) {
            cout << "[Error]Start failed: competition has started.\n";
            return;
        }

        duration_time = duration;
        problem_count = count;
        competition_started = true;

        // Generate problem names (A, B, C, ...)
        for (int i = 0; i < count; i++) {
            problem_names.push_back(string(1, 'A' + i));
        }

        cout << "[Info]Competition starts.\n";
    }

    void submit(const string& problem_name, const string& team_name,
                const string& status_str, int time) {
        Status status = Status::Wrong_Answer; // Default value
        if (status_str == "Accepted") status = Status::Accepted;
        else if (status_str == "Wrong_Answer") status = Status::Wrong_Answer;
        else if (status_str == "Runtime_Error") status = Status::Runtime_Error;
        else if (status_str == "Time_Limit_Exceed") status = Status::Time_Limit_Exceed;

        submissions.push_back({problem_name, team_name, status, time});

        Team& team = teams[team_name];
        TeamProblem& problem = team.problems[problem_name];

        if (problem.solved) {
            // Already solved, ignore subsequent submissions
            return;
        }

        if (status == Status::Accepted) {
            if (!problem.solved) {
                problem.solved = true;
                problem.first_solve_time = time;
                if (!frozen) {
                    team.update_stats(problem_names);
                    ranking_dirty = true;
                }
            }
        } else {
            if (frozen && !problem.solved) {
                problem.frozen_wrong_attempts++;
                problem.is_frozen = true;
            } else {
                problem.wrong_attempts++;
                if (!frozen) {
                    team.update_stats(problem_names);
                    ranking_dirty = true;
                }
            }
        }
    }

    void flush() {
        cout << "[Info]Flush scoreboard.\n";
        update_ranking();
    }

    void freeze() {
        if (frozen) {
            cout << "[Error]Freeze failed: scoreboard has been frozen.\n";
            return;
        }

        frozen = true;
        cout << "[Info]Freeze scoreboard.\n";
    }

    void scroll() {
        if (!frozen) {
            cout << "[Error]Scroll failed: scoreboard has not been frozen.\n";
            return;
        }

        cout << "[Info]Scroll scoreboard.\n";

        // Output scoreboard before scrolling
        update_ranking();
        output_scoreboard();

        // Perform scrolling
        while (true) {
            // Find the lowest ranked team with frozen problems
            Team* target_team = nullptr;
            string target_problem;

            for (int i = ranking.size() - 1; i >= 0; i--) {
                Team* team = ranking[i];
                for (const auto& problem_name : problem_names) {
                    auto& problem = team->problems[problem_name];
                    if (problem.is_frozen) {
                        target_team = team;
                        target_problem = problem_name;
                        break;
                    }
                }
                if (target_team) break;
            }

            if (!target_team) break;

            // Unfreeze the problem
            TeamProblem& problem = target_team->problems[target_problem];
            problem.is_frozen = false;
            problem.wrong_attempts += problem.frozen_wrong_attempts;
            problem.frozen_wrong_attempts = 0;

            // Update team stats
            target_team->update_stats(problem_names);

            // Update ranking
            update_ranking();

            // Find the team that was replaced
            int new_rank = -1;
            for (size_t i = 0; i < ranking.size(); i++) {
                if (ranking[i] == target_team) {
                    new_rank = i + 1;
                    break;
                }
            }

            // Output the ranking change if ranking improved
            if (new_rank > 1) {
                Team* replaced_team = ranking[new_rank - 2];
                cout << target_team->name << " " << replaced_team->name << " "
                     << target_team->solved_count << " " << target_team->total_penalty << "\n";
            }
        }

        frozen = false;

        // Output final scoreboard
        output_scoreboard();
    }

    void query_ranking(const string& team_name) {
        if (teams.find(team_name) == teams.end()) {
            cout << "[Error]Query ranking failed: cannot find the team.\n";
            return;
        }

        cout << "[Info]Complete query ranking.\n";
        if (frozen) {
            cout << "[Warning]Scoreboard is frozen. The ranking may be inaccurate until it were scrolled.\n";
        }

        update_ranking();
        int rank = 1;
        for (const auto* team : ranking) {
            if (team->name == team_name) {
                cout << team_name << " NOW AT RANKING " << rank << "\n";
                return;
            }
            rank++;
        }
    }

    void query_submission(const string& team_name, const string& problem_filter,
                         const string& status_filter) {
        if (teams.find(team_name) == teams.end()) {
            cout << "[Error]Query submission failed: cannot find the team.\n";
            return;
        }

        cout << "[Info]Complete query submission.\n";

        Submission* last_match = nullptr;
        for (auto it = submissions.rbegin(); it != submissions.rend(); ++it) {
            if (it->team_name == team_name) {
                bool problem_match = (problem_filter == "ALL") || (it->problem_name == problem_filter);
                bool status_match = (status_filter == "ALL") ||
                                  (status_to_string(it->status) == status_filter);

                if (problem_match && status_match) {
                    last_match = &(*it);
                    break;
                }
            }
        }

        if (!last_match) {
            cout << "Cannot find any submission.\n";
        } else {
            cout << last_match->team_name << " " << last_match->problem_name << " "
                 << status_to_string(last_match->status) << " " << last_match->time << "\n";
        }
    }

    void end_competition() {
        cout << "[Info]Competition ends.\n";
    }

private:
    void update_ranking() {
        if (!ranking_dirty && !frozen) return;

        ranking.clear();
        for (auto& [name, team] : teams) {
            ranking.push_back(&team);
        }

        sort(ranking.begin(), ranking.end(), [](Team* a, Team* b) {
            return *a < *b;
        });

        ranking_dirty = false;
    }

    void output_scoreboard() {
        for (size_t i = 0; i < ranking.size(); i++) {
            Team* team = ranking[i];
            cout << team->name << " " << (i + 1) << " " << team->solved_count
                 << " " << team->total_penalty;

            for (const auto& problem_name : problem_names) {
                const TeamProblem& problem = team->problems[problem_name];
                cout << " ";

                if (problem.is_frozen) {
                    if (problem.wrong_attempts == 0) {
                        cout << "0/" << problem.frozen_wrong_attempts;
                    } else {
                        cout << "-" << problem.wrong_attempts << "/" << problem.frozen_wrong_attempts;
                    }
                } else if (problem.solved) {
                    if (problem.wrong_attempts == 0) {
                        cout << "+";
                    } else {
                        cout << "+" << problem.wrong_attempts;
                    }
                } else {
                    if (problem.wrong_attempts == 0) {
                        cout << ".";
                    } else {
                        cout << "-" << problem.wrong_attempts;
                    }
                }
            }
            cout << "\n";
        }
    }

    string status_to_string(Status status) {
        switch (status) {
            case Status::Accepted: return "Accepted";
            case Status::Wrong_Answer: return "Wrong_Answer";
            case Status::Runtime_Error: return "Runtime_Error";
            case Status::Time_Limit_Exceed: return "Time_Limit_Exceed";
        }
        return "";
    }
};

int main() {
    ICPCSystem system;
    string line;

    while (getline(cin, line)) {
        if (line.empty()) continue;

        istringstream iss(line);
        string command;
        iss >> command;

        if (command == "ADDTEAM") {
            string team_name;
            iss >> team_name;
            system.add_team(team_name);
        }
        else if (command == "START") {
            string dummy;
            int duration, count;
            iss >> dummy >> duration >> dummy >> count;
            system.start_competition(duration, count);
        }
        else if (command == "SUBMIT") {
            string problem_name, by, team_name, with, status_str, at;
            int time;
            iss >> problem_name >> by >> team_name >> with >> status_str >> at >> time;
            system.submit(problem_name, team_name, status_str, time);
        }
        else if (command == "FLUSH") {
            system.flush();
        }
        else if (command == "FREEZE") {
            system.freeze();
        }
        else if (command == "SCROLL") {
            system.scroll();
        }
        else if (command == "QUERY_RANKING") {
            string team_name;
            iss >> team_name;
            system.query_ranking(team_name);
        }
        else if (command == "QUERY_SUBMISSION") {
            string team_name, where, problem_equals, problem_filter, and_str, status_equals, status_filter;
            iss >> team_name >> where >> problem_equals >> problem_filter >> and_str >> status_equals >> status_filter;

            // Remove "PROBLEM=" and "STATUS=" prefixes
            if (problem_equals.find("PROBLEM=") == 0) {
                problem_filter = problem_equals.substr(8);
            }
            if (status_equals.find("STATUS=") == 0) {
                status_filter = status_equals.substr(7);
            }

            system.query_submission(team_name, problem_filter, status_filter);
        }
        else if (command == "END") {
            system.end_competition();
            break;
        }
    }

    return 0;
}