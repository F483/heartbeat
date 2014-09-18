#
# The MIT License (MIT)
#
# Copyright (c) 2014 William T. James
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

from ..exc import HeartbeatError
from Crypto.Cipher import AES
from Crypto.Hash import HMAC
from Crypto.Hash import SHA256
from Crypto import Random
from Crypto.Util import number


class KeyedPRF(object):
    """KeyedPRF is a psuedo random function. It hashes the input, pads it to
    the correct output length, and then encrypts it with AES. Finally it
    checks that the result is within the desired range. If it is it returns
    the value as a long integer, if it isn't, it increments a nonce in the
    input and recalculates until it finds an integer in the given range
    """
    @staticmethod
    def pad(data, length):
        """This function returns a padded version of the input data to the
        given length.  this function will shorten the given data to the length
        specified if necessary.  post-condition: len(data) = length

        :param data: the data byte array to pad
        :param length: the length to pad the array to
        """
        if (len(data) > length):
            return data[0:length]
        else:
            return data + b"\0"*(length-len(data))

    def __init__(self, key, range):
        """Initialization method

        :param key: the key to use for the PRF.  this key is the only source
        of randomness for the output of this function. should be a hashable
        object, preferably a byte array or string
        :param range: the output range as a long of the function. the output
        of the function will be in [0:range)
        """
        self.key = key
        self.range = range
        # we need a mask because the number we'll generate will be of a
        # certain byte size but we want to restrict it to within a
        # certain bit size because we're trying to find numbers within
        # a certain range this will speed it up
        self.mask = (1 << number.size(self.range))-1

    def eval(self, x):
        """This method returns the evaluation of the function with input x

        :param x: this is the input as a Long
        """
        aes = AES.new(self.key, AES.MODE_CFB, "\0"*AES.block_size)
        while True:
            nonce = 0
            data = KeyedPRF.pad(SHA256.new(str(x+nonce).encode()).digest(),
                                (number.size(self.range)+7)//8)
            num = self.mask & number.bytes_to_long(aes.encrypt(data))
            if (num < self.range):
                return num
            nonce += 1


class Challenge(object):
    """The challenge object that represents a challenge posed to the server
    for proof of storage of a file.
    """
    def __init__(self, chunks, v_max, key):
        """Initialization method

        :param chunks: number of chunks to challenge
        :param v_max: largest coefficient value
        :param key: the key for this challenge
        """
        self.chunks = chunks
        self.v_max = v_max
        self.key = key


class Tag(object):
    """The file tag, generated before uploading by the client.
    """
    def __init__(self):
        """Initialization method
        """
        self.sigma = list()


class State(object):
    """The state which contains two psueod random function keys for generating
    the coeffients for the file tag and verification.
    """
    def __init__(self, f_key, alpha_key, chunks=0,
                 encrypted=False, iv=None, hmac=None):
        """Initialization method

        :param f_key: this is the key for the f psuedo random function
        :param alpha_key: this is the key for the alpha PRF
        :param chunks: the number of chunks in the tagged file
        :param encrypted: whether the state is encrypted (and signed)
        :param iv: the initialization vector for encryption/decryption
        :param hmac: the HMAC signature
        """
        self.f_key = f_key
        self.alpha_key = alpha_key
        self.chunks = chunks
        self.encrypted = encrypted
        self.iv = iv
        self.hmac = hmac

    def encrypt(self, key):
        """This method encrypts and signs the state to make it unreadable by
        the server, since it contains information that would allow faking
        proof of storage.

        :param key: the key to encrypt and sign with
        """
        if (self.encrypted):
            return
        # encrypt
        self.iv = Random.new().read(AES.block_size)
        aes = AES.new(key, AES.MODE_CFB, self.iv)
        self.f_key = aes.encrypt(self.f_key)
        self.alpha_key = aes.encrypt(self.alpha_key)
        # sign
        h = HMAC.new(key, None, SHA256)
        h.update(self.iv)
        h.update(str(self.chunks).encode())
        h.update(self.f_key)
        h.update(self.alpha_key)
        self.hmac = h.digest()
        self.encrypted = True

    def decrypt(self, key):
        """This method checks the signature on the state and decrypts it.

        :param key: the key to decrypt and sign with
        """
        if (not self.encrypted):
            return
        # check signature
        h = HMAC.new(key, None, SHA256)
        h.update(self.iv)
        h.update(str(self.chunks).encode())
        h.update(self.f_key)
        h.update(self.alpha_key)
        if (h.digest() != self.hmac):
            raise HeartbeatError("Signature invalid on state.")
        # decrypt
        aes = AES.new(key, AES.MODE_CFB, self.iv)
        self.f_key = aes.decrypt(self.f_key)
        self.alpha_key = aes.decrypt(self.alpha_key)
        self.encrypted = False


class Proof(object):
    """This class encapsulates proof of storage
    """
    def __init__(self):
        """Initialization method"""
        self.mu = list()
        self.sigma = None


class PySwPriv(object):
    """This class encapsulates the proof of storage engine for the Shacham
    Waters Private scheme.
    """
    def __init__(self, sectors=10, key=None, prime=None, primebits=1024):
        """Initialization method

        :param sectors: the number of sectors to break each chunk into.  this
        allows a trade off between communication complexity and storage
        complexity.  increase the number of sectors to decrease the server
        storage requirement (tag size will decrease) but communication
        complexity will increase
        :param key: the key used for encryption and decryption of the state
        :param prime: the prime to determine the modular group
        :param primebits: optionally the number of bits to use for generation
        of a prime if prime is given as None
        """
        if (key is None):
            self.key = Random.new().read(32)
        else:
            self.key = key
        if (prime is None):
            self.prime = number.getPrime(primebits)
        else:
            self.prime = prime
        self.sectors = sectors
        self.sectorsize = self.prime.bit_length()//8

    def get_public(self):
        """Gets a public version of the object with the key stripped."""
        return PySwPriv(self.sectors, None, self.prime)

    def encode(self, file):
        """This function returns a (tag,state) tuple that is calculated for
        the given file.  the state will be encrypted with `self.key`

        :param file: the file to encode
        """
        tag = Tag()
        tag.sigma = list()

        state = State(Random.new().read(32), Random.new().read(32))

        f = KeyedPRF(state.f_key, self.prime)
        alpha = KeyedPRF(state.alpha_key, self.prime)

        done = False
        chunk_id = 0

        while (not done):
            sigma = f.eval(chunk_id)
            for j in range(0, self.sectors):
                buffer = file.read(self.sectorsize)

                if (len(buffer) > 0):
                    sigma += alpha.eval(j) * number.bytes_to_long(buffer)

                if (len(buffer) != self.sectorsize):
                    done = True
                    break
            sigma %= self.prime
            tag.sigma.append(sigma)
            chunk_id += 1

        state.chunks = chunk_id
        state.encrypt(self.key)

        return (tag, state)

    def gen_challenge(self, state):
        """This function generates a challenge for given state.  It selects a
        random number and sets that as the challenge key.  By default, v_max
        is set to the prime, and the number of chunks to challenge is the
        number of chunks in the file.  (this doesn't guarantee that the whole
        file will be checked since some chunks could be selected twice and
        some selected none.

        :param state: the state to use.  it can be encrypted, as it will
        have just been received from the server
        """
        state.decrypt(self.key)

        chal = Challenge(state.chunks, self.prime, Random.new().read(32))

        return chal

    def prove(self, file, chal, tag):
        """This function returns a proof calculated from the file, the
        challenge, and the file tag

        :param file: this is a file like object that supports `read()`,
        `tell()` and `seek()` methods.
        :param chal: the challenge to use for proving
        :param tag: the file tag
        """
        chunk_size = self.sectors*self.sectorsize

        index = KeyedPRF(chal.key, len(tag.sigma))
        v = KeyedPRF(chal.key, chal.v_max)

        proof = Proof()
        proof.mu = [0]*self.sectors
        proof.sigma = 0

        for j in range(0, self.sectors):
            for i in range(0, chal.chunks):
                pos = index.eval(i) * chunk_size + j * self.sectorsize
                file.seek(pos)
                if (file.tell() == pos):
                    buffer = file.read(self.sectorsize)
                    proof.mu[j] += v.eval(i) * number.bytes_to_long(buffer)
                else:
                    break
            proof.mu[j] %= self.prime

        for i in range(0, chal.chunks):
            proof.sigma += v.eval(i) * tag.sigma[index.eval(i)]

        proof.sigma %= self.prime

        return proof

    def verify(self, proof, chal, state):
        """This returns True if the proof matches the challenge and file state

        :param proof: the proof that was returned from the server
        :param chal: the challenge sent to the server
        :param state: the state of the file, which can be encrypted
        """
        state.decrypt(self.key)

        index = KeyedPRF(chal.key, state.chunks)
        v = KeyedPRF(chal.key, chal.v_max)
        f = KeyedPRF(state.f_key, self.prime)
        alpha = KeyedPRF(state.alpha_key, self.prime)

        rhs = 0

        for i in range(0, chal.chunks):
            rhs += v.eval(i) * f.eval(index.eval(i))

        for j in range(0, self.sectors):
            rhs += alpha.eval(j) * proof.mu[j]

        rhs %= self.prime
        return proof.sigma == rhs
