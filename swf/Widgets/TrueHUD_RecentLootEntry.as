import flash.geom.ColorTransform;
import flash.geom.Transform;

import com.greensock.TimelineLite;
import com.greensock.TweenLite;
import com.greensock.easing.*;

class Widgets.TrueHUD_RecentLootEntry extends MovieClip
{
	private static var DEFAULT_ICON_SOURCE = "skyui/icons_item_psychosteve.swf";
	
	public var ItemText: TextField;
	public var ItemIcon: MovieClip;

	private var iconLoader: MovieClipLoader;
	private var iconSource: String;

	var showHideTimeline: TimelineLite;
	var positionTween: TweenLite;

	var fRecentLootMessageDuration: Number;

	var animDuration = 0.25;

	var bFresh = true;
	var bReadyToRemove = false;

	var iconLabel: String;
	var iconColor: Number;

	public var itemName: String;
	public var itemCount: Number;

	public function TrueHUD_RecentLootEntry() 
	{
		// constructor code
		this.stop();

		if (this._url.indexOf("exported") != -1 || this._url.indexOf("Exported") != -1) 
		{
			this.iconSource = "../" + this.iconSource 
		}

		iconLoader = new MovieClipLoader();
		iconLoader.addListener(this);
		setIcon(undefined, undefined, undefined);
		
		showHideTimeline = new TimelineLite({paused:true});
	}

	private function onLoadInit(a_icon: MovieClip): Void
	{
		updateIcon();

		ItemIcon._width = 24;
		ItemIcon._height = 24;
		ItemIcon._visible = true;
	}
	
	private function setIcon(a_iconSource: String, a_iconLabel: String, a_iconColor: Number) {
		if(!a_iconSource) a_iconSource = DEFAULT_ICON_SOURCE;
		
		if (_url.indexOf("exported") != -1 || _url.indexOf("Exported") != -1) {
			a_iconSource = "../" + a_iconSource;
		}
		
		var iconSourceChanged = a_iconSource != iconSource;
		
		iconSource = a_iconSource;
		iconLabel = a_iconLabel;
		iconColor = a_iconColor;
		
		if(iconSourceChanged) {
			iconLoader.loadClip(iconSource, ItemIcon);
		}
		else {
			updateIcon();
		}
	}

	public function init()
	{
		showHideTimeline.clear();
		showHideTimeline.set(this, {_alpha:0, _x:50}, 0);
		showHideTimeline.to(this, animDuration, {_alpha:100, _x:0}, animDuration);
		showHideTimeline.to(this, animDuration, {_alpha:0}, fRecentLootMessageDuration);
		showHideTimeline.set(this, {bReadyToRemove:true});
		showHideTimeline.eventCallback("onStart", setReadyToRemove, [false], this);
		showHideTimeline.eventCallback("onComplete", setReadyToRemove, [true], this);
	}

	public function cleanUp()
	{
		showHideTimeline.eventCallback("onStart", null);
		showHideTimeline.eventCallback("onComplete", null);
		showHideTimeline.clear();
		showHideTimeline.kill();
		showHideTimeline = null;

		positionTween.kill();
		positionTween = null;
	}

	public function loadConfig(a_fRecentLootMessageDuration: Number)
	{
		fRecentLootMessageDuration = a_fRecentLootMessageDuration;

		init();
	}

	public function updateIcon()
	{
		ItemIcon.gotoAndStop(iconLabel);
		
		var colorTransform = new ColorTransform();
		var transform = new Transform(MovieClip(ItemIcon));

		if(typeof(iconColor) == "number") {
			colorTransform.rgb = iconColor;
		}
		
		transform.colorTransform = colorTransform;
	}

	public function addMessage(a_itemName: String, a_itemCount: Number, a_iconLabel: String, a_iconColor: Number, a_bInstant: Boolean, a_itemData: Object)
	{
		if(a_itemData) {
			// Call i4 if it is installed
			skse.plugins.InventoryInjector.ProcessEntry(a_itemData);
		}
		
		itemName = a_itemName;
		itemCount = a_itemCount;
		
		updateName();
		
		setIcon(a_itemData.iconSource,
				a_itemData.iconLabel ? a_itemData.iconLabel : a_iconLabel,
				a_itemData.iconColor ? a_itemData.iconColor : a_iconColor);
		
		setReadyToRemove(false);
		if (a_bInstant)
		{
			showHideTimeline.play(animDuration);
		}
		else
		{
			showHideTimeline.restart();
		}
		
		bFresh = true;
	}

	public function updateName()
	{
		var name = itemName;
		if (itemCount > 1)
		{
			name = name + " (" + itemCount.toString() + ")";
		}
		ItemText.html = true;
		ItemText.textAutoSize = "shrink";
		ItemText.htmlText = name;
	}

	public function addCount(a_itemCount: Number)
	{
		itemCount = itemCount + a_itemCount;
		updateName();
	}

	public function setY(a_y: Number)
	{
		if (bFresh)
		{
			this._y = a_y;
			bFresh = false;
		}
		else
		{
			positionTween = TweenLite.to(this, animDuration, {_y:a_y, ease:Sine.easeIn});
		}
	}

	public function hideMessage()
	{
		if (showHideTimeline.time() < animDuration + fRecentLootMessageDuration)
		{
			showHideTimeline.play(fRecentLootMessageDuration);
		}
	}

	public function isJustAdded() : Boolean
	{
		return showHideTimeline.time() == 0;
	}

	public function isReadyToRemove() : Boolean
	{
		return bReadyToRemove;
	}

	public function setReadyToRemove(a_readyToRemove: Boolean)
	{
		bReadyToRemove = a_readyToRemove;
	}
}
