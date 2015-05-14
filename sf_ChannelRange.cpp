static const char* const CLASS = "sf_ChannelRange";
static const char* const HELP =
  "Channel Range.\n\n"
  "Node that calculates the range of a channel then modifies the calculated range to a new range (user defined). \n\n"
  "Developed by: Bryan \"Fox\" Dunkley";

#include "DDImage/PixelIop.h"
#include "DDImage/Executable.h"
#include "DDImage/Row.h"
#include "DDImage/Knobs.h"
#include "DDImage/Enumeration_KnobI.h"
#include <algorithm>
#include <sstream>
#include <iostream>
#include <math.h>

using namespace DD::Image;
using namespace std;

static const char* const default_channel_list[] = {"none          " , 0};

class sf_ChannelRangeIop : public PixelIop, Executable
{
  int channel_selection;
  int mask_selection;
  int chansel_hack;
  int masksel_hack;
  std::vector<std::string> channel_list;
  float seq_max_val;
  float seq_min_val;
  float seq_new_min_val;
  float seq_new_max_val;
  float min_val;
  float max_val;
  bool init_min_and_max;
  bool use_mask_channel;
  Channel chan, mask_chan;
  LABEL min_max_info;
  float gamma;

public:

  sf_ChannelRangeIop(Node* node) : PixelIop(node), Executable(this)
  {
	  channel_selection = 0;
	  mask_selection = 0;
	  chansel_hack = 0;
	  masksel_hack = 0;
	  seq_max_val = 1.0f;
	  seq_min_val = 0.0f;
	  seq_new_min_val = 0.0f;
	  seq_new_max_val = 1.0f;
	  min_val = 0.0f;
	  max_val = 1.0f;
	  init_min_and_max = false;
	  use_mask_channel = false;
	  chan = Chan_Black;
	  mask_chan = Chan_Black;
	  min_max_info = "max = None, min = None";
	  gamma = 1.0f;
  }
  
  void in_channels(int input_number, ChannelSet& channels) const {}
  
  void channelListGenerator();
  //void append(Hash& hash);
  virtual void knobs(Knob_Callback);
  int knob_changed(Knob* k);
  const char* Class() const { return CLASS; }
  const char* node_help() const { return HELP; }
  const char* node_shape() const { return "[]";}
  static const Iop::Description d;
  //executable crap
  Executable* executable();
  void beginExecuting();
  void execute();
  void endExecuting();
  //
  void scanFrame();
  const char* labelProcessing(bool usingMasks);
  void _validate(bool);
  void pixel_engine(const Row &in, int y, int x, int r, ChannelMask, Row &out);
};

void sf_ChannelRangeIop::channelListGenerator()
{
	channel_list.clear();
	channel_list.push_back("none       ");

  if (node_input( 0, OUTPUT_OP) != NULL)
  {
	  ChannelMask inputchannels = input0().channels();

	  ostringstream s;

	  foreach(c, inputchannels)
	  {
		  s << c;
		  channel_list.push_back(s.str());
		  s.clear();
		  s.str("");
	  }
  }
}

/*void sf_ChannelRangeIop::append(Hash& hash)
{
   int count = outputContext().frame();
   hash.append(count);
}*/

const char* sf_ChannelRangeIop::labelProcessing(bool usingMasks)
{
	string label, channel, mask, name;
	ostringstream s;

	s << knob("name")->get_text();
	name = s.str();
	s.clear();
	s.str("");

	if (usingMasks)
	{
		if (masksel_hack != 0)
		{
			s << chan;
			channel = s.str();
			s.clear();
			s.str("");

			s << mask_chan;
			mask = s.str();
			s.clear();
			s.str("");

			channel = channel.substr(channel.find(".") + 1,channel.length());
			mask = mask.substr(mask.find(".") + 1,mask.length());

			label = "\"" + name + "\\n(" + channel + " / " + mask + ")\"";
		}
	}
	else
	{
		if (chansel_hack != 0)
		{
			s << chan;
			channel = s.str();
			s.clear();
			s.str("");

			s << knob("name")->get_text();
			name = s.str();
			s.clear();
			s.str("");

			channel = channel.substr(channel.find(".") + 1,channel.length());
			label = "\"" + name + "\\n(" + channel + ")\"";
		}
		else
		{
			label = "\"" + name + "\"";
		}
	}

	return label.c_str();
}


void sf_ChannelRangeIop::scanFrame() //draw specified channel
{
	input0().validate(true);
	input0().request(chan, 0);

	int x = input0().x();
	int r = input0().r();
	int y = input0().y();
	int t = input0().t();

	for( int curRow = y; curRow < t; curRow++ )
	{
		Row row ( x, r );
		input(0)->get( curRow, x, r, chan, row );

		for (int i = x; i < r; i++)
		{
			const float* vIn = row[chan] + i;
			const float* aIn = row[mask_chan] + i;

			if (use_mask_channel)
			{
				if (*aIn > 0.0f)
				{
					if (!init_min_and_max)
					{
						//initialization of min and max to first value of frame
						seq_max_val = *vIn * *aIn;
						seq_min_val = *vIn * *aIn;
						init_min_and_max = true;
					}
					else
					{
						// do some analysis here
						if ( (*vIn * *aIn) < seq_min_val)
						{
							seq_min_val = *vIn * *aIn;
						}

						if ( (*vIn * *aIn) > seq_max_val)
						{
							seq_max_val = *vIn * *aIn;
						}
					}
				}
			}
			else
			{
				if (!init_min_and_max)
				{
					//initialization of min and max to first value of frame
					seq_max_val = *vIn;
					seq_min_val = *vIn;
					init_min_and_max = true;
				}
				else
				{
					// do some analysis here
					if (*vIn < seq_min_val)
					{
						seq_min_val = *vIn;
					}

					if (*vIn > seq_max_val)
					{
						seq_max_val = *vIn;
					}
				}
			}

			if (Op::aborted())
			break;
		}

	}
}

void sf_ChannelRangeIop::knobs(Knob_Callback f)
{
	Int_knob(f, &chansel_hack,"chanhack", "chanhack");
	SetFlags(f, Knob::INVISIBLE);
	Int_knob(f, &masksel_hack,"maskhack", "maskhack");
	SetFlags(f, Knob::INVISIBLE);

	Enumeration_knob(f, &channel_selection, default_channel_list, "channelSelector", "channel");
	SetFlags(f, Knob::DO_NOT_WRITE);

	Enumeration_knob(f, &mask_selection, default_channel_list, "maskSelector", "mask");
	SetFlags(f, Knob::DO_NOT_WRITE);

	SetFlags(f, Knob::STARTLINE);
	Bool_knob(f, &use_mask_channel, "use_mask", "use mask");

	Divider(f,"");
	Script_knob(f, "execute_panel this", "cal_min_max", " calculate channel range");
	SetFlags(f, Knob::STARTLINE);
	Button(f,"reset","reset");

	Divider(f,"From Range");
	Float_knob(f, &min_val, IRange(0, 1), "org_min_value", "min value");
	Float_knob(f, &max_val, IRange(0, 1), "org_max_value", "max value");

	Divider(f,"To Range");
	Float_knob(f, &seq_new_min_val, IRange(0, 1), "new_min_value", "min value");
	Float_knob(f, &seq_new_max_val, IRange(0, 1), "new_max_value", "max value");

	Divider(f,"");
	Float_knob(f, &gamma, IRange(0.2f, 5.0f), "gamma_knob", "gamma (R)");
}

int sf_ChannelRangeIop::knob_changed(Knob* k)
{
	Enumeration_KnobI * enum_interface = knob("channelSelector")->enumerationKnob();
	Enumeration_KnobI * mask_enum_interface = knob("maskSelector")->enumerationKnob();

	channelListGenerator();

	enum_interface->menu(channel_list);
	mask_enum_interface->menu(channel_list);

	if ((k->name() == "maskSelector") || (k->name() == "channelSelector"))
	{
		knob("chanhack")->set_value(knob("channelSelector")->get_value());
		knob("maskhack")->set_value(knob("maskSelector")->get_value());
	}

	knob("channelSelector")->set_value(knob("chanhack")->get_value());
	knob("maskSelector")->set_value(knob("maskhack")->get_value());

	knob("channelSelector")->updateWidgets();
	knob("maskSelector")->updateWidgets();

	chan = getChannel(enum_interface->menu()[knob("chanhack")->get_value()].c_str());
	mask_chan = getChannel(enum_interface->menu()[knob("maskhack")->get_value()].c_str());

	if (k->name() == "reset")
	{
		seq_min_val = 0.0f;
		seq_max_val = 1.0f;
		seq_new_min_val = 0.0f;
		seq_new_max_val = 1.0f;
		min_val = 0.0f;
		max_val = 1.0f;
		init_min_and_max = false;
		knob("org_min_value")->set_value(seq_min_val);
		knob("org_max_value")->set_value(seq_max_val);
	}

	if ((k->name() == "maskSelector") && (mask_selection != 0) && !use_mask_channel)
	{
		knob("use_mask")->set_value(true);
		use_mask_channel = true;
		knob("autolabel")->set_text(labelProcessing(true));
	}

	if ((k->name() == "maskSelector") && (mask_selection == 0) && use_mask_channel)
	{
		knob("use_mask")->set_value(false);
		use_mask_channel = false;
		knob("autolabel")->set_text(labelProcessing(false));
	}

	knob("autolabel")->set_text(labelProcessing(use_mask_channel));

	return 1;
}

static Iop* build(Node* node) { return new sf_ChannelRangeIop(node); }
const Iop::Description sf_ChannelRangeIop::d(CLASS, 0, build);

Executable* sf_ChannelRangeIop::executable()
{
	return this;
}

void sf_ChannelRangeIop::beginExecuting()
{
	//cout << "beginExecuting" << endl;
	init_min_and_max = false;
}

void sf_ChannelRangeIop::execute()
{
	outputContext().frame();
	scanFrame();

	knob("org_min_value")->set_value(seq_min_val);
	knob("org_max_value")->set_value(seq_max_val);

	knob("org_min_value")->changed();
	knob("org_max_value")->changed();

	//Hash ophash = hash();
	//append(ophash);
}

void sf_ChannelRangeIop::endExecuting()
{
	//cout << "endExecuting\n" << endl;

	knob("org_min_value")->set_value(seq_min_val);
	knob("org_max_value")->set_value(seq_max_val);
	
	knob("org_min_value")->changed();
	knob("org_max_value")->changed();
}


void sf_ChannelRangeIop::_validate(bool for_real)
{
	channelListGenerator();
	if (chansel_hack != 0)
	{
		//Enumeration_KnobI* enum_interface = knob("channelSelector")->enumerationKnob();
		//chan = getChannel(enum_interface->menu()[chansel_hack].c_str());
		//cout << chan << endl;
		set_out_channels(Mask_All);
	}
	else
	{
		set_out_channels(Mask_None);
	}
    PixelIop::_validate(for_real);
}

void sf_ChannelRangeIop::pixel_engine(const Row &in, int y, int x, int r, ChannelMask channels, Row &out)
{
		//Basic algorithm used for modifying ranges
		//OldRange = (OldMax - OldMin)
		//NewRange = (NewMax - NewMin)
		//NewValue = (((OldValue - OldMin) * NewRange) / OldRange) + NewMin

		foreach (z, channels)
		{
			float OldRange = (max_val - min_val);
			float NewRange = (seq_new_max_val - seq_new_min_val);

			 const float* In = in[z] + x;
			 float* Out = out.writable(z) + x;

			 // Pointer to when the loop is done:
			 const float* END = In + (r - x);

			while (In < END)
			{	
				float oldValue = *In++;
				float newRangeValue = (((oldValue - min_val) * NewRange) / OldRange) + seq_new_min_val;

				////////////  GAMMA   ///////////////
				if (gamma <= 0)
				{
					if (newRangeValue < 1.0f)
						newRangeValue = 0.0f;
					else if (newRangeValue > 1.0f)
						newRangeValue = INFINITY;

				    *Out++ = newRangeValue;
				}
				else if (gamma != 1.0f)
				{
					if (newRangeValue <= 0.0f)
					{

					}
				    else if (newRangeValue <= 1e-6f && gamma > 1.0f)
				    {
				    	newRangeValue = 0.0f;
				    }
				    else if (newRangeValue < 1.0f)
					{
				    	newRangeValue = powf(newRangeValue, (gamma));
					}
					else
					{
						newRangeValue = 1.0f + (newRangeValue - 1.0f) * (gamma);
					}

					*Out++ = newRangeValue;
				}
				else
				{
					*Out++ = newRangeValue;
				}
				//////////////////////
			}
		}
}
