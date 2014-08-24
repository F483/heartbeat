import sys
import timeit
sys.path.append("..")
from heartbeat import Heartbeat


# Config Vars
file_path = "../files/test.txt"
file_path2 = "../files/test2.txt"
file_path3 = "../files/test3.txt"
size_path = "../files/test4.txt"
root_seed = "myroot"


# Unit Test
def unit_test():

    # Create challenges from file
    file1 = Heartbeat(file_path)
    file1.generate_challenges(10, root_seed)
    challenge = file1.random_challenge

    # Create hash_response from seed and duplicate file
    file2 = Heartbeat(file_path2)
    answer = file2.meet_challenge(challenge)

    # Check to see if they match
    assert(file1.check_answer(answer))

    # Create hash_answer from seed and edited file
    file3 = Heartbeat(file_path3)
    answer = file3.meet_challenge(challenge)

    # This should not match
    assert(not file1.check_answer(answer))


# Unit Test on a Custom File
def custom_unit_test(file_path):
    # Create challenges from file
    file1 = Heartbeat(file_path)
    file1.generate_challenges(10, root_seed)
    challenge = file1.random_challenge


# Size Tests
def size_test():
    # Time and Size of Challenges
    print("Month of Challenges (1 per hour):")
    print(str(timeit.timeit(size1, number=1)) + " seconds")
    print("Year of Challenges (1 per hour):")
    print(str(timeit.timeit(size2, number=1)) + " seconds")
    print("")


def num_challenges(number):
    file1 = Heartbeat(size_path)
    file1.generate_challenges(number, root_seed)
    print("Size: " + str(file1.challenges_size /1024) + " kb")


def size1():
    num_challenges(1000) # 731 hours in a month


def size2():
    num_challenges(10000) # 8766 hours in a year


if __name__ == "__main__":
    try:
        unit_test()
        if (len(sys.argv) == 2):
            custom_unit_test(sys.argv[1])
        size_test()
    except AssertionError:
        print("Failed Unit Testing...")
    else:
        print("Passed Unit Testing...")
