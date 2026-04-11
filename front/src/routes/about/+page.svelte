<script>
	import Card from '$lib/components/Card.svelte';
	import Tag from '$lib/components/Tag.svelte';
</script>

<Card tag="section">
	<h1>About</h1>

	<p>
		Date.now() is a weekly show on <a href="https://twitch.tv/devgirl_">Twitch</a> every Thursday
		morning, in <Tag --margin="0 4px">French</Tag>. Although, the newsletters and all the content
		outside Twitch is in <Tag --margin="0 4px">English</Tag>
	</p>

	<p>
		We cover cybersecurity, web and software development, AI ethics, and much more — including the
		remarkable women who shaped the history of computer science.
	</p>

	<p>
		If you missed a show on Twitch, don't hesitate to watch the VODs on YouTube. <Tag
			--margin="0 4px">French</Tag
		>
	</p>

	<h2>How do I prepare an edition?</h2>

	<p>
		First of all, I am susbcribe to <em>a lot of newsletters</em> to be able to stay up-to-date. If
		you are interesting in the newsletters that I use, check this page --
		<a href="/used-newsletters">Newsletters that I use</a>
	</p>
</Card>
