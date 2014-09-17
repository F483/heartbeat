/*

The MIT License (MIT)

Copyright (c) 2014 William T. James

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

*/

#pragma once

// must include cmath before python to avoid clash of hypot later.
#include <cmath>
#include <Python.h>
#include <CXX/Objects.hxx>
#include <CXX/Extensions.hxx>
#include <iostream>
#include "serializable.hxx"
#include "shacham_waters_private.hxx"
#include "PyBytesSink.hxx"
#include "PythonSeekableFile.hxx"
#include "PyArray.hxx"
#include <sstream>
#include "base64.h"

namespace SwPriv
{

// This encapsulates a wrapper class for a serializable class
template<typename T>
class PyBytesStateAccessible : public T
{
public:
	enum encoding_type { binary = 0, base64 = 1 };
	
	PyBytesStateAccessible() : _encoding(binary) {}

	py_array get_state()
	{
		// serialize the underlying and output
		if (_encoding == binary)
		{
			PyBytesSink sink;
			this->serialize(sink);
			return sink.finish();
		}
		else
		{
			std::string bin;
			CryptoPP::StringSink sink(bin);
			this->serialize(sink);
			std::string b64 = base64_encode((const unsigned char*)bin.c_str(),bin.size());
			return py_array(b64);
		}
	}
	
	void set_state(py_array state)
	{
		CryptoPP::StringSource *ss;
		if (_encoding == binary)
		{
			ss = new CryptoPP::StringSource(state,true);
		}
		else
		{
			std::string bin = base64_decode(state.as_std_string());
			ss = new CryptoPP::StringSource(bin,true);
		}
		// deserializep takes ownership of the stringsource and deletes it after use, so we do not have to cleanup StringSource
		this->deserializep(ss);
	}
	
	encoding_type get_encoding() const { return _encoding; }
	void set_encoding(encoding_type encoding) { _encoding = encoding; }
	
private:
	encoding_type _encoding;
};

template<typename Tthis, typename Tbase>
class PyBytesStateAccessiblePyClass : public Py::PythonClass<Tthis>, public PyBytesStateAccessible<Tbase>
{
public:
	typedef PyBytesStateAccessiblePyClass<Tthis,Tbase> this_type;

	PyBytesStateAccessiblePyClass( Py::PythonClassInstance *self, Py::Tuple &args, Py::Dict &kwds )
		: Py::PythonClass< Tthis >::PythonClass( self, args, kwds ) 
	{
		//std::cout << "PyBytesStateAccessiblePyClass<Tthis,Tbase> constructor called." << std::endl;
	}

	static void init_type_dont_ready(const char *T_type_name,const char *doc = "")
	{
		behaviors().name(T_type_name);
		behaviors().doc(doc);
		
		Tthis::PYCXX_ADD_NOARGS_METHOD( __getstate__, _get_state, "__getstate__()\nReturns the state of this object for serialization." );
		Tthis::PYCXX_ADD_VARARGS_METHOD(  __setstate__, _set_state, "__setstate__( state )\nTakes the state as returned by __getstate__ as an argument.  Sets the object's internal state to that specified.");
		Tthis::PYCXX_ADD_NOARGS_METHOD(  __reduce__, _reduce, "__reduce__()\nReduces the object and returns a tuple of the callable constructor, any arguments to that constructor, and the object state.");
	}
	
	static void init_type(const char *T_type_name,const char *doc = "")
	{
		init_type_dont_ready(T_type_name,doc);
		
		behaviors.readyType();
	}
	
	Py::Object getattro( const Py::String &name_ )
    {
        return this->genericGetAttro( name_ );
    }
	
	int setattro( const Py::String &name_, const Py::Object &value )
    {
        return this->genericSetAttro( name_, value );
    }
	
	Py::Object _get_state()
	{
		try 
		{
			return this->get_state();
		}
		catch (const std::exception &e)
		{
			throw Py::RuntimeError(e.what());
		}
		return Py::None();
	}
	PYCXX_NOARGS_METHOD_DECL( Tthis, _get_state )
	
	Py::Object _set_state(const Py::Tuple &args )
	{
		if (args.length() != 1)
		{
			throw Py::RuntimeError("__setstate__ only takes one argument: state");
		}
		try 
		{
			this->set_state(args[0]);
		}
		catch (const std::exception &e)
		{
			throw Py::RuntimeError(e.what());
		}
		return Py::None();
	}
	PYCXX_VARARGS_METHOD_DECL( Tthis, _set_state )
	
	Py::Object _reduce()
	{
		return Py::TupleN( Tthis::type(), Py::TupleN(), _get_state() );
	}
	PYCXX_NOARGS_METHOD_DECL( Tthis, _reduce )
};

class Tag : public PyBytesStateAccessiblePyClass<Tag,shacham_waters_private_data::tag>
{
public:
	Tag( Py::PythonClassInstance *self, Py::Tuple &args, Py::Dict &kwds )
		: PyBytesStateAccessiblePyClass<Tag,shacham_waters_private_data::tag>(self,args,kwds) 
	{
		//std::cout << "Tag construtor called." << std::endl;
	}
	
	static void init_type()
	{
		PyBytesStateAccessiblePyClass<Tag,shacham_waters_private_data::tag>::init_type_dont_ready("heartbeat.SwPriv.Tag",
"This object represents a file tag which should be stored on the server, and is used for \
construction of a proof of storage.");
		
		behaviors().readyType();
	}
};

class State : public PyBytesStateAccessiblePyClass<State,shacham_waters_private_data::state>
{
public:
	State( Py::PythonClassInstance *self, Py::Tuple &args, Py::Dict &kwds )
		: PyBytesStateAccessiblePyClass<State,shacham_waters_private_data::state>(self,args,kwds) 
	{
		//std::cout << "State construtor called." << std::endl;
	}
	
	static void init_type()
	{
		PyBytesStateAccessiblePyClass<State,shacham_waters_private_data::state>::init_type_dont_ready("heartbeat.SwPriv.State",
"This object represents a file state which should be stored on the server.  It will automatically be encrypted\
if it is generated by the SwPriv.encode() method.  It should be retrieved for generation of the challenge.");
		
		PYCXX_ADD_VARARGS_METHOD( encrypt, _encrypt, "encrypt(encryption_key,hmac_key)\nEncrypts the internal state for serialization.  Encryption and decryption is handled automatically by the SwPriv class." );
		PYCXX_ADD_VARARGS_METHOD( decrypt, _decrypt, "decrypt(encryption_key,hmac_key)\nDecrypts the internal state after deserialization.  Encryption and decryption are handled automatically by the SwPriv class." );
		PYCXX_ADD_NOARGS_METHOD( keysize, _keysize, "keysize()\nReturns the size of the key used for encryption and decryption of the state.  Using a non-standard key size will not work." );
		
		behaviors().readyType();
	}
	
	Py::Object _encrypt(const Py::Tuple &args)
	{
		try 
		{
			byte *key_enc;
			ssize_t enc_sz;
			byte *key_mac;
			ssize_t mac_sz;
			if (args.size() < 2)
			{
				throw Py::RuntimeError("encrypt() takes at least two arguments: the encryption key and the mac key and an optional argument a bool, whether to use convergent encryption");
			}
			
			convert_and_check_key(args[0],&key_enc,&enc_sz);
			convert_and_check_key(args[1],&key_mac,&mac_sz);
			
			bool use_convergent = false;
			if (args.size() > 2 && args[2].isTrue())
			{
				use_convergent = true;
			}
			
			encrypt_and_sign(key_enc,key_mac,use_convergent);
		}
		catch (const std::exception &e)
		{
			throw Py::RuntimeError(e.what());
		}
		return Py::None();
	}
	PYCXX_VARARGS_METHOD_DECL( State, _encrypt )
	
	Py::Object _decrypt(const Py::Tuple &args)
	{
		try
		{
			byte *key_enc;
			ssize_t enc_sz;
			byte *key_mac;
			ssize_t mac_sz;
			if (args.size() != 2)
			{
				throw Py::RuntimeError("decrypt() takes two arguments: the encryption key and the mac key.");
			}
			
			convert_and_check_key(args[0],&key_enc,&enc_sz);
			convert_and_check_key(args[1],&key_mac,&mac_sz);
			
			check_sig_and_decrypt(key_enc,key_mac);
		}
		catch (const std::exception &e)
		{
			throw Py::RuntimeError(e.what());
		}
		return Py::None();
	}
	PYCXX_VARARGS_METHOD_DECL( State, _decrypt )
	
	Py::Object _keysize()
	{
		return Py::Long((long)shacham_waters_private_data::key_size);
	}
	PYCXX_NOARGS_METHOD_DECL( State, _keysize )
	
private:
	void convert_and_check_key(Py::Object in_key,byte **out_key,ssize_t *out_sz)
	{
		if (py_as_string_and_size(in_key.ptr(),(char**)out_key,out_sz))
		{
			throw Py::RuntimeError("Invalid encryption key.");
		}
		if (*out_sz != shacham_waters_private_data::key_size)
		{
			std::stringstream ss;
			ss << "Encryption key must be " << shacham_waters_private_data::key_size << " bytes in length.  Use keysize() to retrieve the key size." << std::endl;
			throw Py::RuntimeError(ss.str());
		}
	}
};

class Challenge : public PyBytesStateAccessiblePyClass<Challenge,shacham_waters_private_data::challenge>
{
public:
	Challenge( Py::PythonClassInstance *self, Py::Tuple &args, Py::Dict &kwds )
		: PyBytesStateAccessiblePyClass<Challenge,shacham_waters_private_data::challenge>(self,args,kwds) 
	{
		//std::cout << "Challenge construtor called." << std::endl;
	}
	
	static void init_type()
	{
		PyBytesStateAccessiblePyClass<Challenge,shacham_waters_private_data::challenge>::init_type_dont_ready("heartbeat.SwPriv.Challenge",
"This object represents the challenge sent from client to server to verify proof of storage of a file.");
		
		behaviors().readyType();
	}
};

class Proof : public PyBytesStateAccessiblePyClass<Proof,shacham_waters_private_data::proof>
{
public:
	Proof( Py::PythonClassInstance *self, Py::Tuple &args, Py::Dict &kwds )
		: PyBytesStateAccessiblePyClass<Proof,shacham_waters_private_data::proof>(self,args,kwds) 
	{
		//std::cout << "Proof construtor called." << std::endl;
	}
	
	static void init_type()
	{
		PyBytesStateAccessiblePyClass<Proof,shacham_waters_private_data::proof>::init_type_dont_ready("heartbeat.SwPriv.Proof",
"This object represents proof of storage of the file which is sent from the server to the client.");
		
		behaviors().readyType();
	}
};


class SwPriv : public PyBytesStateAccessiblePyClass<SwPriv,shacham_waters_private>
{
public:
	SwPriv( Py::PythonClassInstance *self, Py::Tuple &args, Py::Dict &kwds )
		: PyBytesStateAccessiblePyClass<SwPriv,shacham_waters_private>(self,args,kwds) 
	{
		//std::cout << "SwPriv constructor called..." << std::endl;
		try 
		{
			gen();
			
			//std::cout << "heartbeat generated." << std::endl;
		}
		catch (const std::exception &e)
		{
			throw Py::RuntimeError(e.what());
		}
	}

	static void init_type()
	{
		PyBytesStateAccessiblePyClass<SwPriv,shacham_waters_private>::init_type_dont_ready("heartbeat.SwPriv.SwPriv",
"This class contains methods for verification of storage of a file on a remote\n\
server.  The internal state of this object is necessary for the client to\n\
decrypt the state in order to verify the proof sent from the remote server.\n\
Before sending this object to the server it is necessary to get the public\n\
version which strips the encryption keys for the state.  If the remote server\n\
has access to these keys, the remote server can pretend to hold the file.\n\
This class provides the following methods for verification of proof of storage:\n\
get_public() which returns the public version of this object which is stripped\n\
of the secret verification data.\n\
encode(file) which returns a tuple (tag,state) for sending to the remote server.\n\
The state information will be encrypted and is ready for serialization.\n\
gen_challenge(state) which returns a challenge for sending to the server.  The\n\
state should be retreived from the server and this function will decrypt it and\n\
verify its signature before generating a challenge.  Upon failure it will raise\n\
a runtime error.\n\
prove(file,challenge,tag) which returns a proof that should be sent back to\n\
the client for verification.\n\
verify(proof,challenge,state) which returns a boolean representing whether the\n\
proof is valid given the challenge and file state.  This function will decypt\n\
the state if necessary.\n");
		
		PYCXX_ADD_NOARGS_METHOD( get_public, _get_public, "get_public()\nReturns the public version of this object which is stripped\n\
of the secret verification data." );
		PYCXX_ADD_VARARGS_METHOD( encode, _encode, "(tag,state) = encode(file)\nReturns a tuple (tag,state) for sending to the remote server.\n\
The state information will be encrypted and is ready for serialization." );
		PYCXX_ADD_VARARGS_METHOD( gen_challenge, _gen_challenge, "challenge = gen_challenge(state)\nReturns a challenge for sending to the server.  The\n\
state should be retreived from the server and this function will decrypt it and\n\
verify its signature before generating a challenge.  Upon failure it will raise\n\
a runtime error." );
		PYCXX_ADD_VARARGS_METHOD( prove, _prove, "proof = prove(file,challenge,tag)\nReturns a proof that should be sent back to\n\
the client for verification." );
		PYCXX_ADD_VARARGS_METHOD( verify, _verify, "is_valid = verify(proof,challenge,state)\nReturns a boolean representing whether the\n\
proof is valid given the challenge and file state. This function will decypt\n\
the state if necessary." );
		
		behaviors().readyType();
	}
	
	Py::Object _get_public()
	{
		try 
		{
			Py::Callable class_type( SwPriv::type() );
			Py::PythonClassObject<SwPriv> pyobj( class_type.apply( Py::Tuple() ) );
			SwPriv *obj = pyobj.getCxxObject();
		
			get_public(*obj);
			
			//std::cout << "public heartbeat retrieved." << std::endl;
		
			return pyobj;
		}
		catch (const std::exception &e)
		{
			throw Py::RuntimeError(e.what());
			return Py::None();
		}
	}
	PYCXX_NOARGS_METHOD_DECL( SwPriv, _get_public )
	
	// (tag,state) = encode(file)
	Py::Object _encode(const Py::Tuple &args )
	{
		try 
		{
			//std::cout << "encoding file...";
			
			Py::Callable tag_type( Tag::type() );
			Py::PythonClassObject<Tag> pytag( tag_type.apply( Py::Tuple() ) );
			Tag *tag = pytag.getCxxObject();
			
			Py::Callable state_type( State::type() );
			Py::PythonClassObject<State> pystate( state_type.apply( Py::Tuple() ) );
			State *state = pystate.getCxxObject();
			
			PythonSeekableFile psf(args[0]);
			
			//std::cout << "stream file generated..." << std::endl;
			
			encode(*tag,*state,psf);
			
			//std::cout << "done" << std::endl;
			
			return Py::TupleN(pytag,pystate);
		} 
		catch (const std::exception &e)
		{
			throw Py::RuntimeError(e.what());
			return Py::None();
		}
	}
	PYCXX_VARARGS_METHOD_DECL( SwPriv, _encode )
	
	// challenge = gen_challenge(state)
	Py::Object _gen_challenge(const Py::Tuple &args )
	{
		try
		{
			//std::cout << "generating challenge...";
			
			Py::Callable challenge_type( Challenge::type() );
			Py::PythonClassObject<Challenge> pychallenge( challenge_type.apply( Py::Tuple() ) );
			Challenge *challenge = pychallenge.getCxxObject();
		
			Py::PythonClassObject<State> pystate( args[0] );
			State *state = pystate.getCxxObject();
		 
			gen_challenge(*challenge,*state);
		
			//std::cout << "done." << std::endl;
			
			return pychallenge;
		}
		catch (const std::exception &e)
		{
			throw Py::RuntimeError(e.what());
			return Py::None();
		}
	}
	PYCXX_VARARGS_METHOD_DECL( SwPriv, _gen_challenge )
	
	// proof = public_beat.prove(file,challenge,tag)
	Py::Object _prove(const Py::Tuple &args )
	{
		try
		{
			PythonSeekableFile psf(args[0]);
			
			Py::PythonClassObject<Challenge> pychallenge( args[1] );
			Challenge *challenge = pychallenge.getCxxObject();
			
			Py::PythonClassObject<Tag> pytag( args[2] );
			Tag *tag = pytag.getCxxObject();
			
			Py::Callable proof_type( Proof::type() );
			Py::PythonClassObject<Proof> pyproof( proof_type.apply( Py::Tuple() ) );
			Proof *proof = pyproof.getCxxObject();
			
			prove(*proof,psf,*challenge,*tag);
			
			return pyproof;
		}
		catch (const std::exception &e)
		{
			throw Py::RuntimeError(e.what());
			return Py::None();
		}
	}
	PYCXX_VARARGS_METHOD_DECL( SwPriv, _prove )
	
	// is_valid = beat.verify(proof,challenge,state)
	Py::Object _verify(const Py::Tuple &args )
	{
		try
		{
			Py::PythonClassObject<Proof> pyproof( args[0] );
			Proof *proof = pyproof.getCxxObject();
		
			Py::PythonClassObject<Challenge> pychallenge( args[1] );
			Challenge *challenge = pychallenge.getCxxObject();
			
			Py::PythonClassObject<State> pystate( args[2] );
			State *state = pystate.getCxxObject();
			
			bool is_valid = verify(*proof,*challenge,*state);
			
			if (is_valid)
			{
				return Py::True();
			}
			else
			{
				return Py::False();
			}
		}
		catch (const std::exception &e)
		{
			throw Py::RuntimeError(e.what());
			return Py::None();
		}
		catch (const Py::Exception &)
		{
			return Py::None();
		}
	}
	PYCXX_VARARGS_METHOD_DECL( SwPriv, _verify )
};

}

class Module : public Py::ExtensionModule<Module>
{
public:
	Module()
		: Py::ExtensionModule<Module>("SwPriv")
	{
		SwPriv::SwPriv::init_type();
		SwPriv::State::init_type();
		SwPriv::Tag::init_type();
		SwPriv::Challenge::init_type();
		SwPriv::Proof::init_type();
		
		initialize(
"This module implements a private homomorphic linear authentication scheme for proof of storage.\
The main class SwPriv contains the worker functions for encoding files,\n\
generating challenges and proofs, and verifying those proofs.\n");
		
		Py::Dict d( moduleDictionary() );
		d["SwPriv"] = Py::Object(SwPriv::SwPriv::type());
		d["State"] = Py::Object(SwPriv::State::type());
		d["Tag"] = Py::Object(SwPriv::Tag::type());
		d["Challenge"] = Py::Object(SwPriv::Challenge::type());
		d["Proof"] = Py::Object(SwPriv::Proof::type());
	}
};