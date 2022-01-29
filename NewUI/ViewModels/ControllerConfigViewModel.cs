﻿using Mesen.Config;
using ReactiveUI;
using ReactiveUI.Fody.Helpers;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Reactive.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Mesen.ViewModels
{
	public class ControllerConfigViewModel : ViewModelBase
	{
		[Reactive] public ControllerConfig Config { get; set; }

		[Reactive] public KeyMappingViewModel KeyMapping1 { get; set; }
		[Reactive] public KeyMappingViewModel KeyMapping2 { get; set; }
		[Reactive] public KeyMappingViewModel KeyMapping3 { get; set; }
		[Reactive] public KeyMappingViewModel KeyMapping4 { get; set; }

		[Reactive] public bool ShowPresets { get; set; } = false;
		[Reactive] public bool ShowTurbo { get; set; } = false;

		[Obsolete("For designer only")]
		public ControllerConfigViewModel() : this(new ControllerConfig()) { }

		public ControllerConfigViewModel(ControllerConfig config)
		{
			Config = config;

			KeyMapping1 = new KeyMappingViewModel(config.Type, config.Mapping1);
			KeyMapping2 = new KeyMappingViewModel(config.Type, config.Mapping2);
			KeyMapping3 = new KeyMappingViewModel(config.Type, config.Mapping3);
			KeyMapping4 = new KeyMappingViewModel(config.Type, config.Mapping4);

			ShowPresets = config.Type == ControllerType.SnesController || config.Type == ControllerType.NesController || config.Type == ControllerType.GameboyController;
			ShowTurbo = config.Type == ControllerType.SnesController || config.Type == ControllerType.NesController || config.Type == ControllerType.GameboyController;
		}
	}

	public class KeyMappingViewModel : ViewModelBase
	{
		[Reactive] public ControllerType Type { get; set; }
		[Reactive] public KeyMapping Mapping { get; set; }
		[Reactive] public List<CustomKeyMapping> CustomKeys { get; set; } = new();

		[Obsolete("For designer only")]
		public KeyMappingViewModel() : this(ControllerType.ExcitingBoxing, new()) { }

		public KeyMappingViewModel(ControllerType type, KeyMapping mapping)
		{
			Type = type;
			Mapping = mapping;

			CustomKeys = Mapping.ToCustomKeys(type);
		}

		public void RefreshCustomKeys()
		{
			CustomKeys = Mapping.ToCustomKeys(Type);
		}
	}

	public class CustomKeyMapping : ViewModelBase
	{
		public string Name { get; set; }
		public UInt32[] Mappings { get; set; }
		public int Index { get; set; }
		[Reactive] public UInt32 KeyMapping { get; set; }

		public CustomKeyMapping(string name, UInt32[] mappings, int index)
		{
			Name = name;
			Mappings = mappings;
			Index = index;
			KeyMapping = mappings[index];

			this.WhenAnyValue(x => x.KeyMapping).Subscribe(x => {
				mappings[index] = x;
			});
		}
	}
}
