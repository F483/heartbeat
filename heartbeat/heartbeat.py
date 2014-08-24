#!/usr/bin/env python
# -*- coding: utf-8 -*-

import sys
import copy
import random
import hashlib
import os.path

from .exc import HeartbeatError


class Challenge(object):
    """ The Challenge class represents a challenge one node can pose to
    another when requesting verification that they have a complete version
    of a specific file.
    """

    def __init__(self, block, seed):
        """ Initialization method

        :param block:
        :param seed:
        """
        self.block = block
        self.seed = seed
        self.response = None

    @property
    def without_answer(self):
        """ Provide a challenge for sending to other nodes. """
        new = copy.copy(self)
        del new.response
        return new


class Heartbeat(object):
    """ A small library used to create and verify hash challenges
    so Node A can verify that Node B has a specified file.
    """

    def __init__(self, filepath):
        # Check if the file exists
        """ Initialization method

        :param filepath: Valid path to file
        :raise HeartbeatError: If file path does not exist
        """
        if os.path.isfile(filepath):
            self.file_size = os.path.getsize(filepath)
            self.file_object = open(filepath, "rb")
        else:
            raise HeartbeatError("%s not found" % filepath)

        # Challenges is a list of 2-tuples (seed, hash_response)
        self.challenges = []

    def generate_challenges(self, num, root_seed):
        """ Generate the specified number of hash challenges.

        Arguments:
        num -- The number of hash challenges we want to generate.
        root_seed -- Some value that we use to generate our seeds from.
        """

        # Generate a series of seeds
        seeds = self.generate_seeds(num, root_seed)
        blocks = self.pick_blocks(num, root_seed)

        # List of 2-tuples (seed, hash_response)
        self.challenges = []

        # Generate the corresponding hash for each seed
        for i in range(num):
            self.challenges.append(Challenge(blocks[i], seeds[i]))
            response = self.meet_challenge(self.challenges[i])
            self.challenges[i].response = response

    def meet_challenge(self, challenge):
        """ Get the SHA256 hash of a specific file block plus the provided
        seed. The default block size is one tenth of the file. If the file is
        larger than 10KB, 1KB is used as the block size.

        :param challenge: challenge as a `Challenge <heartbeat.Challenge>`
        object
        """
        chunk_size = min(1024, self.file_size // 10)
        try:
            seed = bytes(str(challenge.seed), 'utf-8')
        except TypeError:
            seed = bytes(str(challenge.seed))

        h = hashlib.sha256()
        self.file_object.seek(challenge.block)

        if challenge.block > (self.file_size - chunk_size):
            end_slice = (
                challenge.block - (self.file_size - chunk_size)
            )
            h.update(self.file_object.read(end_slice))
            self.file_object.seek(0)
            h.update(self.file_object.read(chunk_size - end_slice))
        else:
            h.update(self.file_object.read(chunk_size))

        h.update(seed)

        return h.hexdigest()

    @staticmethod
    def generate_seeds(num, root_seed):
        """ Deterministically generate list of seeds from a root seed.

        :param num: Numbers of seeds to generate as int
        :param root_seed: Seed to start off with.
        """
        # Generate a starting seed from the root
        seeds = []
        random.seed(root_seed)
        tmp_seed = random.random()

        # Deterministically generate the rest of the seeds
        for x in range(num):
            seeds.append(tmp_seed)
            random.seed(tmp_seed)
            tmp_seed = random.random()

        return seeds

    def pick_blocks(self, num, root_seed):
        """
        Pick a set of positions to start reading blocks from the
        file that challenges are created for.

        Positions are guaranteed to be within the bounds of the file.
        """
        blocks = []
        random.seed(root_seed)

        for i in range(num):
            blocks.append(random.randint(0, self.file_size - 1))

        return blocks

    def check_answer(self, hash_answer):
        """
        Check if the returned hash is in our challenges list.

        Arguments:
        hash_answer -- a hash that we compare to our list of challenges.
        """
        for a_challenge in self.challenges:
            if a_challenge.get_response() == hash_answer:
                # If we don't disgard a used challenge then a node
                # could fake having the file because it already
                # knows the proper response
                # self.delete_challenge(hash_answer)
                return True
        return False

    def delete_challenge(self, hash_answer):
        """Delete challenge from our list of challenges."""
        for a_challenge in self.challenges:
            if a_challenge.get_response() == hash_answer:
                self.challenges.remove(a_challenge)
                return True
        return False

    def get_challenge(self):
        """Get a random challenge."""
        return random.choice(self.challenges).get_without_answer()

    def challenges_size(self):
        """Get bytes size of our challenges."""
        return sys.getsizeof(self.challenges)
