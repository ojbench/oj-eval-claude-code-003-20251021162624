#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <sstream>
#include <unordered_map>
#include <functional>

using namespace std;

enum class JudgeStatus {
    Accepted,
    Wrong_Answer,
    Runtime_Error,
    Time_Limit_Exceed
};

struct Submission {
    string problem_name;
    string team_name;
    JudgeStatus status;
    int time;
};

struct ProblemStatus {
    bool solved = false;
    int first_solve_time = -1;
    int wrong_attempts_before_ac = 0;
    int frozen_attempts = 0;
    bool is_frozen = false;

    int get_penalty() const {
        if (!solved) return 0;
        return 20 * wrong_attempts_before_ac + first_solve_time;
    }

    int get_total_attempts() const {
        return wrong_attempts_before_ac + frozen_attempts;
    }
};

struct Team {
    string name;
    unordered_map<string, ProblemStatus> problems;
    int solved_count = 0;
    int total_penalty = 0;
    vector<int> solve_times;

    void calculate_stats(const vector<string>& problem_names) {
        solved_count = 0;
        total_penalty = 0;
        solve_times.clear();

        for (const auto& pname : problem_names) {
            const auto& problem = problems[pname];
            if (problem.solved && !problem.is_frozen) {
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
    bool started = false;
    bool frozen = false;
    int duration = 0;
    int problem_count = 0;
    vector<string> problem_names;
    unordered_map<string, Team> teams;
    vector<Submission> submissions;
    vector<Team*> ranking;
    bool needs_ranking_update = true;

public:
    void add_team(const string& team_name) {
        if (started) {
            cout << "[Error]Add failed: competition has started.\n";
            return;
        }
        if (teams.count(team_name)) {
            cout << "[Error]Add failed: duplicated team name.\n";
            return;
        }
        teams[team_name] = Team();
        teams[team_name].name = team_name;
        cout << "[Info]Add successfully.\n";
    }

    void start_competition(int dur, int count) {
        if (started) {
            cout << "[Error]Start failed: competition has started.\n";
            return;
        }
        duration = dur;
        problem_count = count;
        started = true;
        for (int i = 0; i < count; i++) {
            problem_names.push_back(string(1, 'A' + i));
        }
        cout << "[Info]Competition starts.\n";
    }

    void submit(const string& problem, const string& team,
                const string& status_str, int time) {
        JudgeStatus status = JudgeStatus::Wrong_Answer;
        if (status_str == "Accepted") status = JudgeStatus::Accepted;
        else if (status_str == "Wrong_Answer") status = JudgeStatus::Wrong_Answer;
        else if (status_str == "Runtime_Error") status = JudgeStatus::Runtime_Error;
        else if (status_str == "Time_Limit_Exceed") status = JudgeStatus::Time_Limit_Exceed;

        submissions.push_back({problem, team, status, time});

        Team& t = teams[team];
        ProblemStatus& ps = t.problems[problem];

        if (ps.solved) return; // Ignore submissions after solving

        if (status == JudgeStatus::Accepted) {
            ps.solved = true;
            ps.first_solve_time = time;
            if (!frozen) {
                t.calculate_stats(problem_names);
                needs_ranking_update = true;
            }
        } else {
            if (frozen) {
                ps.frozen_attempts++;
                ps.is_frozen = true;
            } else {
                ps.wrong_attempts_before_ac++;
                t.calculate_stats(problem_names);
                needs_ranking_update = true;
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

        // Output initial scoreboard
        update_ranking();
        print_scoreboard();

        // Process scrolling
        while (true) {
            // Find lowest ranked team with frozen problems
            Team* target_team = nullptr;
            string target_problem;

            for (int i = ranking.size() - 1; i >= 0; i--) {
                Team* team = ranking[i];
                for (const auto& pname : problem_names) {
                    auto& problem = team->problems[pname];
                    if (problem.is_frozen) {
                        target_team = team;
                        target_problem = pname;
                        break;
                    }
                }
                if (target_team) break;
            }

            if (!target_team) break;

            // Unfreeze the problem
            ProblemStatus& problem = target_team->problems[target_problem];
            problem.is_frozen = false;
            problem.wrong_attempts_before_ac += problem.frozen_attempts;
            problem.frozen_attempts = 0;

            // Update team stats
            target_team->calculate_stats(problem_names);

            // Update ranking
            update_ranking();

            // Check if ranking changed
            int new_rank = -1;
            for (size_t i = 0; i < ranking.size(); i++) {
                if (ranking[i] == target_team) {
                    new_rank = i + 1;
                    break;
                }
            }

            // Output ranking change if improved
            if (new_rank > 1) {
                Team* replaced_team = ranking[new_rank - 2];
                cout << target_team->name << " " << replaced_team->name << " "
                     << target_team->solved_count << " " << target_team->total_penalty << "\n";
            }
        }

        frozen = false;

        // Output final scoreboard
        print_scoreboard();
    }

    void query_ranking(const string& team_name) {
        if (!teams.count(team_name)) {
            cout << "[Error]Query ranking failed: cannot find the team.\n";
            return;
        }

        cout << "[Info]Complete query ranking.\n";
        if (frozen) {
            cout << "[Warning]Scoreboard is frozen. The ranking may be inaccurate until it were scrolled.\n";
        }

        update_ranking();
        for (size_t i = 0; i < ranking.size(); i++) {
            if (ranking[i]->name == team_name) {
                cout << team_name << " NOW AT RANKING " << (i + 1) << "\n";
                return;
            }
        }
    }

    void query_submission(const string& team_name, const string& problem_filter,
                         const string& status_filter) {
        if (!teams.count(team_name)) {
            cout << "[Error]Query submission failed: cannot find the team.\n";
            return;
        }

        cout << "[Info]Complete query submission.\n";

        // Find last matching submission
        for (auto it = submissions.rbegin(); it != submissions.rend(); ++it) {
            if (it->team_name == team_name) {
                bool problem_ok = (problem_filter == "ALL") || (it->problem_name == problem_filter);
                bool status_ok = (status_filter == "ALL") ||
                               (status_to_string(it->status) == status_filter);

                if (problem_ok && status_ok) {
                    cout << it->team_name << " " << it->problem_name << " "
                         << status_to_string(it->status) << " " << it->time << "\n";
                    return;
                }
            }
        }

        cout << "Cannot find any submission.\n";
    }

    void end_competition() {
        cout << "[Info]Competition ends.\n";
    }

private:
    void update_ranking() {
        if (!needs_ranking_update && !frozen) return;

        ranking.clear();
        for (auto& [name, team] : teams) {
            ranking.push_back(&team);
        }

        sort(ranking.begin(), ranking.end(), [](Team* a, Team* b) {
            return *a < *b;
        });

        needs_ranking_update = false;
    }

    void print_scoreboard() {
        for (size_t i = 0; i < ranking.size(); i++) {
            Team* team = ranking[i];
            cout << team->name << " " << (i + 1) << " " << team->solved_count
                 << " " << team->total_penalty;

            for (const auto& pname : problem_names) {
                const ProblemStatus& problem = team->problems[pname];
                cout << " ";

                if (problem.is_frozen) {
                    if (problem.wrong_attempts_before_ac == 0) {
                        cout << "0/" << problem.frozen_attempts;
                    } else {
                        cout << "-" << problem.wrong_attempts_before_ac << "/" << problem.frozen_attempts;
                    }
                } else if (problem.solved) {
                    if (problem.wrong_attempts_before_ac == 0) {
                        cout << "+";
                    } else {
                        cout << "+" << problem.wrong_attempts_before_ac;
                    }
                } else {
                    int total = problem.get_total_attempts();
                    if (total == 0) {
                        cout << ".";
                    } else {
                        cout << "-" << total;
                    }
                }
            }
            cout << "\n";
        }
    }

    string status_to_string(JudgeStatus status) {
        switch (status) {
            case JudgeStatus::Accepted: return "Accepted";
            case JudgeStatus::Wrong_Answer: return "Wrong_Answer";
            case JudgeStatus::Runtime_Error: return "Runtime_Error";
            case JudgeStatus::Time_Limit_Exceed: return "Time_Limit_Exceed";
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
        string cmd;
        iss >> cmd;

        if (cmd == "ADDTEAM") {
            string team;
            iss >> team;
            system.add_team(team);
        }
        else if (cmd == "START") {
            string dummy;
            int dur, count;
            iss >> dummy >> dur >> dummy >> count;
            system.start_competition(dur, count);
        }
        else if (cmd == "SUBMIT") {
            string problem, by, team, with, status, at;
            int time;
            iss >> problem >> by >> team >> with >> status >> at >> time;
            system.submit(problem, team, status, time);
        }
        else if (cmd == "FLUSH") {
            system.flush();
        }
        else if (cmd == "FREEZE") {
            system.freeze();
        }
        else if (cmd == "SCROLL") {
            system.scroll();
        }
        else if (cmd == "QUERY_RANKING") {
            string team;
            iss >> team;
            system.query_ranking(team);
        }
        else if (cmd == "QUERY_SUBMISSION") {
            string team, where, prob_eq, prob_filter, and_str, stat_eq, stat_filter;
            iss >> team >> where >> prob_eq >> prob_filter >> and_str >> stat_eq >> stat_filter;

            // Parse problem and status filters
            if (prob_eq.find("PROBLEM=") == 0) {
                prob_filter = prob_eq.substr(8);
            }
            if (stat_eq.find("STATUS=") == 0) {
                stat_filter = stat_eq.substr(7);
            }

            system.query_submission(team, prob_filter, stat_filter);
        }
        else if (cmd == "END") {
            system.end_competition();
            break;
        }
    }

    return 0;
}